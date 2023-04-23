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

#define SNAKE_BUFFER_SIZE 20

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

// -------------------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------------------
// Screens
static Screen screens[SCREEN_COUNT];
static ScreenState currentScreen, nextScreen;

static const Vector2 dirVectors[] = {
    {0.0f, 0.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}};

static Vector2 snake[SNAKE_BUFFER_SIZE];
static int snakeHead, snakeTail;
static float snakeTimer, snakeSpeed;
static Direction snakeDir;

static Vector2 apple;

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
Vector2 GeneratePoint(void);
void DrawBlock(float fading, float x, float y, Color color);
void RenderGrid(float fading);

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

    snakeHead = 2;
    snakeTail = 0;
    snake[0] = (Vector2){0, 0};
    snake[1] = (Vector2){GRID_WIDTH, 0};
    snake[2] = (Vector2){2 * GRID_WIDTH, 0};
    snakeTimer = 0;
    snakeSpeed = 5; // blocks per second
    snakeDir = DIR_RIGHT;

    apple = GeneratePoint();
}

void UpdateGameScreen(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetNextScreen(SCREEN_MENU);
    }

    if (IsKeyPressed(KEY_UP)) {
        snakeDir = DIR_UP;
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        snakeDir = DIR_RIGHT;
    }
    if (IsKeyPressed(KEY_DOWN)) {
        snakeDir = DIR_DOWN;
    }
    if (IsKeyPressed(KEY_LEFT)) {
        snakeDir = DIR_LEFT;
    }

    snakeTimer += dt;
    if (snakeTimer > 1.0f / snakeSpeed) {
        snakeTimer -= 1.0f / snakeSpeed;

        // New head
        int previousHead = snakeHead;
        snakeHead = (snakeHead + 1) % SNAKE_BUFFER_SIZE;
        snake[snakeHead].x =
            snake[previousHead].x + dirVectors[snakeDir].x * GRID_WIDTH;
        snake[snakeHead].y =
            snake[previousHead].y + dirVectors[snakeDir].y * GRID_HEIGHT;

        // eat apple
        if ((int)snake[snakeHead].x == (int)apple.x &&
            (int)snake[snakeHead].y == (int)apple.y) {
            apple = GeneratePoint();
            snakeSpeed *= 1.1f;
        } else {
            // pop tail
            snakeTail = (snakeTail + 1) % SNAKE_BUFFER_SIZE;
        }
    }
}

void RenderGameScreen(float fading) {
    ClearBackground(BLACK);

    // debug drawing
    RenderGrid(fading);

    // render apple
    DrawBlock(fading, apple.x, apple.y, GREEN);

    // draw apple
    int tail = snakeTail;
    while (tail != snakeHead) {
        Vector2 snakePart = snake[tail];
        DrawBlock(fading, snakePart.x, snakePart.y, WHITE);
        tail = (tail + 1) % SNAKE_BUFFER_SIZE;
    }

    // draw head
    DrawBlock(fading, snake[snakeHead].x, snake[snakeHead].y, WHITE);
}

void InitAssets(void) { ChangeDirectory(ASSET_PATH); }

void DestroyAssets(void) {}

void DrawBlock(float fading, float x, float y, Color color) {
    int gridX = ((int)x / GRID_WIDTH) * GRID_WIDTH;
    int gridY = ((int)y / GRID_HEIGHT) * GRID_HEIGHT;
    Rectangle rect = {gridX, gridY, GRID_WIDTH, GRID_HEIGHT};
    Rectangle innerRect = {gridX + GRID_MARGIN, gridY + GRID_MARGIN,
                           GRID_WIDTH - 2 * GRID_MARGIN, GRID_HEIGHT - 2 * GRID_MARGIN};
    DrawRectangleLinesEx(rect, 1.0, Fade(color, fading));
    DrawRectangleRec(innerRect, Fade(color, fading));
}

Vector2 GeneratePoint(void) {
    Vector2 point;

    point.x = GetRandomValue(0, SCREEN_WIDTH / GRID_WIDTH - 1) * GRID_WIDTH;
    point.y = GetRandomValue(0, SCREEN_HEIGHT / GRID_HEIGHT - 1) * GRID_HEIGHT;

    return point;
}

void RenderGrid(float fading) {
    Color gridColor = {20, 20, 20, 255};
    int linesWidth, linesHeight;

    gridColor = Fade(gridColor, fading);
    linesWidth = SCREEN_WIDTH / GRID_WIDTH;
    linesHeight = SCREEN_HEIGHT / GRID_HEIGHT;

    for (int y = 0; y < linesHeight; ++y) {
        for (int x = 0; x < linesWidth; ++x) {
            DrawBlock(fading, x * GRID_WIDTH, y * GRID_HEIGHT, gridColor);
        }
    }
}
