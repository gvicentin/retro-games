#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#ifndef ASSET_PATH
#define ASSET_PATH "./assets"
#endif

// Screen constants
#define SCREEN_TITLE     "Snake"
#define SCREEN_WIDTH     800
#define SCREEN_HEIGHT    600
#define SCREEN_FADE_TIME 0.3f

#define GRID_WIDTH  20
#define GRID_HEIGHT 20
#define GRID_MARGIN 3

#define MAX_PIECES 100

// -------------------------------------------------------------------------------------
// Enumerations
// -------------------------------------------------------------------------------------
typedef enum {
    SCREEN_NONE = 0,
    SCREEN_MENU,
    SCREEN_GAME,
    SCREEN_COUNT
} ScreenState;

typedef enum {
    DIR_NONE = 0,
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT,
    DIR_COUNT
} Direction;

// -------------------------------------------------------------------------------------
// Structs
// -------------------------------------------------------------------------------------
typedef struct Screen {
    void (*init)(void);
    void (*update)(float dt);
    void (*render)(float fading);
    bool hasFinished;
} Screen;

typedef struct {
    Vector2 position;
    Vector2 size;
    Direction direction;

    // turning point
    Vector2 nextPoint;
    Direction nextDir;
} SnakePiece;

// -------------------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------------------
// Screens
static Screen screens[SCREEN_COUNT];
static ScreenState currentScreen, nextScreen;

static const Vector2 m_dirVectors[] = {
    {0.0f, 0.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}};

SnakePiece m_soldiers[MAX_PIECES];
int m_soldiersCount;
float m_speed;
float m_accum, m_frameDelay;

SnakePiece m_newSoldier;

// -------------------------------------------------------------------------------------
// Module declaration
// -------------------------------------------------------------------------------------
// Screen management
void InitScreen(ScreenState initialScreen);
void SetNextScreen(ScreenState state);
bool ScreenShouldClose(void);
void UpdateScreen(void);

// Menu screen
void InitMenuScreen(void);
void UpdateMenuScreen(float dt);
void RenderMenuScreen(float fading);

// Game screen
void InitGameScreen(void);
void UpdateGameScreen(float dt);
void RenderGameScreen(float fading);

// Helper functions
void InitAssets(void);
void DestroyAssets(void);
void InitSnakePiece(SnakePiece *snakePiece, Vector2 pos, Direction dir);
bool IsValidDirection(Direction nextDir, Direction oposeDir);
bool IsReadyToTurn(SnakePiece *snakePiece);
Vector2 GetLeaderPoint(void);
Vector2 GeneratePoint(void);
void DrawSnakePiece(float fading, float x, float y, Color color);

// -------------------------------------------------------------------------------------
// Entrypoint
// -------------------------------------------------------------------------------------
int main(void) {

// pre configuration
#if defined(DEBUG)
    SetTraceLogLevel(LOG_DEBUG);
#else
    SetTraceLogLevel(LOG_NONE);
#endif

    // initialization
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_TITLE);
    InitScreen(SCREEN_MENU);
    InitAudioDevice();
    InitAssets();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateScreen, 0, 1);
#else
    // pos configuration, must happen after window creation
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // gameloop
    while (!WindowShouldClose() && !ScreenShouldClose()) {
        UpdateScreen();
    }
#endif

    // cleanup
    DestroyAssets();
    CloseAudioDevice();
    CloseWindow();

    return 0;
}

// -------------------------------------------------------------------------------------
// Module implementation
// -------------------------------------------------------------------------------------
Screen CreateScreen(ScreenState screenState) {
    Screen screen;
    screen.hasFinished = false;

    switch (screenState) {
    case SCREEN_MENU:
        screen.init = &InitMenuScreen;
        screen.update = &UpdateMenuScreen;
        screen.render = &RenderMenuScreen;
        break;
    case SCREEN_GAME:
        screen.init = &InitGameScreen;
        screen.update = &UpdateGameScreen;
        screen.render = &RenderGameScreen;
        break;
    default:
        screen.init = NULL;
        screen.update = NULL;
        screen.render = NULL;
    }

    return screen;
}

void InitScreen(ScreenState initialScreen) {
    // create screens
    for (int i = 0; i < SCREEN_COUNT; ++i) {
        screens[i] = CreateScreen(i);
    }

    // set initial screen
    currentScreen = initialScreen;
    nextScreen = SCREEN_NONE;
    screens[currentScreen].init();
}

void SetNextScreen(ScreenState screen) {
    screens[currentScreen].hasFinished = true;
    nextScreen = screen;
}

bool ScreenShouldClose(void) {
    return screens[currentScreen].hasFinished && nextScreen == SCREEN_NONE;
}

void UpdateScreen(void) {
    static bool isFadingIn = true;
    static bool isFadingOut = false;
    static float fadingDir = 1.0f, fading = 0.0f;

    float dt = GetFrameTime();

    // update screen
    if (!isFadingIn && !isFadingOut) {
        screens[currentScreen].update(dt);
    } else {
        fading += dt * fadingDir;
    }

    // render game
    BeginDrawing();
    ClearBackground(BLACK);
    screens[currentScreen].render(fading / SCREEN_FADE_TIME);
    EndDrawing();

    if (screens[currentScreen].hasFinished) {
        isFadingOut = true;
    }

    if (isFadingIn && fabsf(fading) > SCREEN_FADE_TIME) {
        isFadingIn = false;
        fading = SCREEN_FADE_TIME;
        fadingDir = -1.0f;
    }

    if (isFadingOut && fabsf(fading) > SCREEN_FADE_TIME) {
        // reset previous
        screens[currentScreen].hasFinished = false;
        isFadingOut = false;
        isFadingIn = true;
        fading = 0.0f;
        fadingDir = 1.0f;

        // start new
        currentScreen = nextScreen;
        nextScreen = SCREEN_NONE;
        screens[currentScreen].init();
    }
}

void InitMenuScreen(void) { TraceLog(LOG_DEBUG, "Menu Screen"); }

void UpdateMenuScreen(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetNextScreen(SCREEN_NONE);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        SetNextScreen(SCREEN_GAME);
    }
}

void RenderMenuScreen(float fading) {
    int titleMeasure = MeasureText("SNAKE", 64);
    DrawText("SNAKE", (SCREEN_WIDTH - titleMeasure) / 2.0f, 140, 64,
             Fade(WHITE, fading));
}

void InitGameScreen(void) {
    TraceLog(LOG_DEBUG, "Game Screen");
    SetRandomSeed(time(NULL));

    m_soldiersCount = 3;
    for (int i = 0; i < m_soldiersCount; ++i) {
        Vector2 initialPos = {(m_soldiersCount - i) * GRID_WIDTH, 0};
        InitSnakePiece(&m_soldiers[i], initialPos, DIR_RIGHT);
    }

    m_speed = 50.0f;
    m_accum = 0.0f;
    m_frameDelay = 0.1f;

    Vector2 newSoldierPos = GeneratePoint();
    InitSnakePiece(&m_newSoldier, newSoldierPos, DIR_NONE);
}

void UpdateGameScreen(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetNextScreen(SCREEN_MENU);
    }
    m_accum += dt;

    Direction nextDir = DIR_NONE;
    if (IsKeyPressed(KEY_UP) && IsValidDirection(DIR_UP, DIR_DOWN)) {
        nextDir = DIR_UP;
    }
    if (IsKeyPressed(KEY_RIGHT) && IsValidDirection(DIR_RIGHT, DIR_LEFT)) {
        nextDir = DIR_RIGHT;
    }
    if (IsKeyPressed(KEY_DOWN) && IsValidDirection(DIR_DOWN, DIR_UP)) {
        nextDir = DIR_DOWN;
    }
    if (IsKeyPressed(KEY_LEFT) && IsValidDirection(DIR_LEFT, DIR_RIGHT)) {
        nextDir = DIR_LEFT;
    }

    if (nextDir != DIR_NONE) {
        m_soldiers[0].nextDir = nextDir;
        m_soldiers[0].nextPoint = GetLeaderPoint();
    }

    Vector2 dir, vel;

    for (int i = m_soldiersCount - 1; i >= 0; --i) {
        SnakePiece *soldier = &m_soldiers[i];

        // lerp size
        soldier->size =
            Vector2Lerp(soldier->size, (Vector2){GRID_WIDTH, GRID_HEIGHT}, 10.0f * dt);

        dir = m_dirVectors[soldier->direction];
        vel = Vector2Scale(dir, m_speed * dt);
        soldier->position = Vector2Add(soldier->position, vel);

        if (soldier->nextDir != DIR_NONE && IsReadyToTurn(soldier)) {
            // reached turning position
            if (i < m_soldiersCount - 1) {
                // update soldier behind before turning
                SnakePiece *soldierBehind = &m_soldiers[i + 1];
                soldierBehind->nextDir = soldier->nextDir;
                soldierBehind->nextPoint = soldier->nextPoint;
            }

            // making the turn
            soldier->position = soldier->nextPoint;
            soldier->direction = soldier->nextDir;
            soldier->nextDir = DIR_NONE;
        }
    }

    Vector2 leaderPos = GetLeaderPoint();
    if (Vector2Equals(leaderPos, m_newSoldier.position)) {
        m_newSoldier.position = GeneratePoint();

        SnakePiece *newSoldier = &m_soldiers[m_soldiersCount];
        SnakePiece lastSoldier = m_soldiers[m_soldiersCount - 1];
        *newSoldier = lastSoldier;
        ++m_soldiersCount;

        switch (lastSoldier.direction) {
        case DIR_UP:
            newSoldier->position.y += GRID_HEIGHT;
            break;
        case DIR_RIGHT:
            newSoldier->position.x -= GRID_WIDTH;
            break;
        case DIR_DOWN:
            newSoldier->position.y -= GRID_HEIGHT;
            break;
        case DIR_LEFT:
            newSoldier->position.x += GRID_WIDTH;
            break;
        default:
            break;
        }

        // sizing
        newSoldier->size = (Vector2){GRID_WIDTH / 5.0f, GRID_HEIGHT / 5.0f};
    }
}

void RenderGameScreen(float fading) {
    ClearBackground(BLACK);

    // draw soldiers 
    for (int i = 0; i < m_soldiersCount; ++i) {
        SnakePiece soldier = m_soldiers[i];
        DrawSnakePiece(fading, soldier.position.x, soldier.position.y, WHITE);
    }

    // draw new soldier
    DrawSnakePiece(fading, m_newSoldier.position.x, m_newSoldier.position.y, GREEN);

    // debug drawing
    // RenderGrid();

    // draw leader
    // DrawRectangleLines(m_soldiers[0].position.x, m_soldiers[0].position.y, GRID_WIDTH, GRID_HEIGHT, BLUE);
}

void InitAssets(void) { ChangeDirectory(ASSET_PATH); }

void DestroyAssets(void) {}

void DrawSnakePiece(float fading, float x, float y, Color color) {
    int gridX = ((int) x / GRID_WIDTH) * GRID_WIDTH;
    int gridY = ((int) y / GRID_HEIGHT) * GRID_HEIGHT;
    Rectangle rect = {gridX, gridY, GRID_WIDTH, GRID_HEIGHT};
    Rectangle innerRect = {gridX + GRID_MARGIN, gridY + GRID_MARGIN,
                           GRID_WIDTH - 2 * GRID_MARGIN, GRID_HEIGHT - 2 * GRID_MARGIN};
    DrawRectangleLinesEx(rect, 1.0, Fade(color, fading));
    DrawRectangleRec(innerRect, Fade(color, fading));
}

void InitSnakePiece(SnakePiece *soldier, Vector2 pos, Direction dir) {
    soldier->position = pos;
    soldier->size = (Vector2) { GRID_WIDTH, GRID_HEIGHT };
    soldier->direction = dir;
    soldier->nextDir = DIR_NONE;
    soldier->nextPoint = soldier->position;
}

bool IsValidDirection(Direction nextDir, Direction oposeDir) {
    SnakePiece leader = m_soldiers[0];
    return nextDir != DIR_NONE && nextDir != leader.direction
        && oposeDir != leader.direction;
}

bool IsReadyToTurn(SnakePiece *soldier) {
    switch (soldier->direction) {
        case DIR_UP:
            return FloatEquals(soldier->position.x, soldier->nextPoint.x) && 
                soldier->position.y < soldier->nextPoint.y;
        case DIR_RIGHT:
            return soldier->position.x > soldier->nextPoint.x &&
                FloatEquals(soldier->position.y, soldier->nextPoint.y);
        case DIR_DOWN:
            return FloatEquals(soldier->position.x, soldier->nextPoint.x) && 
                soldier->position.y > soldier->nextPoint.y;
        case DIR_LEFT:
            return soldier->position.x < soldier->nextPoint.x &&
                FloatEquals(soldier->position.y, soldier->nextPoint.y);
        default:
            return false;
    }
}

Vector2 GetLeaderPoint(void) {
    SnakePiece leader = m_soldiers[0];
    Vector2 point = leader.position;

    point.x = leader.direction == DIR_RIGHT ? point.x + GRID_WIDTH : point.x;
    point.y = leader.direction == DIR_DOWN ? point.y + GRID_HEIGHT : point.y;

    point.x = floorf(point.x/GRID_WIDTH)*GRID_WIDTH;
    point.y = floorf(point.y/GRID_HEIGHT)*GRID_HEIGHT;

    return point;
}

Vector2 GeneratePoint(void) {
    Vector2 point;

    point.x = GetRandomValue(0, SCREEN_WIDTH/GRID_WIDTH - 1)*GRID_WIDTH;
    point.y = GetRandomValue(0, SCREEN_HEIGHT/GRID_HEIGHT - 1)*GRID_HEIGHT;

    return point;
}

static void RenderGrid(void) {
    int linesWidth, linesHeight;

    linesWidth = SCREEN_WIDTH/GRID_WIDTH;
    linesHeight = SCREEN_HEIGHT/GRID_HEIGHT;

    for (int i = 0; i < linesWidth; ++i) {
        // draw vertical lines
        DrawLine(i*GRID_WIDTH, 0, i*GRID_WIDTH, SCREEN_HEIGHT, WHITE);
    }

    for (int i = 0; i < linesHeight; ++i) {
        // draw horizontal lines
        DrawLine(0, i*GRID_HEIGHT, SCREEN_WIDTH, i*GRID_HEIGHT, WHITE);
    }
}
