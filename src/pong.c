#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define SCREEN_TITLE  "Pong"
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

#define COLOR_BG      BLACK
#define COLOR_FG WHITE

#define SCREEN_FADE_TIME 0.3f

#define PADDLE_WIDTH      20
#define PADDLE_HEIGHT     80
#define PADDLE_SPEED      600
#define PADDLE_IA_SPEED   400
#define PADDLE_HOR_OFFSET 30

#define BALL_WIDTH           15
#define BALL_HEIGHT          15
#define BALL_INITIAL_SPEED   400
#define BALL_SPEED_INCREMENT 100

#define BORDER_WIDTH 15
#define LIMIT_TOP    BORDER_WIDTH
#define LIMIT_RIGHT  (SCREEN_WIDTH - PADDLE_HOR_OFFSET)
#define LIMIT_BOTTOM (SCREEN_HEIGHT - BORDER_WIDTH)
#define LIMIT_LEFT   PADDLE_HOR_OFFSET

#define BOUNCE_POINTS_MAX 20

typedef enum { SCREEN_NONE = 0, SCREEN_MENU, SCREEN_GAME, SCREEN_COUNT } ScreenState;

typedef struct Screen {
    void (*init)(void);
    void (*update)(float dt);
    void (*render)(void);
    bool hasFinished;
} Screen;

typedef struct Entity {
    Rectangle rect; // position and dimensions
    Vector2 dir;    // normilized direction
    float speed;    // velocity multiplier
} Entity;

typedef struct CollisionData {
    bool hit;              // true if collision has happened
    float time;            // time for collision [0.0,1.0]
    Vector2 contactPoint;  // collision point for restitution
    Vector2 contactNormal; // surface normal where collide
} CollisionData;

static Screen screens[SCREEN_COUNT];
static ScreenState currentScreen, nextScreen;
static float screenFadeTimer;

static bool debugMode;

static int leftScore, rightScore;

static Entity leftPaddle, rightPaddle, ball;
static int hitCounter;

static Vector2 bouncePoints[BOUNCE_POINTS_MAX];
static int bouncePointsCount;

static float iaTargetPos, iaHitPos, iaResponseTime, iaTimer;

// start and end line points
static Vector2 topSP, rightSP, bottomSP, leftSP;
static Vector2 topEP, rightEP, bottomEP, leftEP;

// -----------------------------------------------------------------------------
// Module declaration
// -----------------------------------------------------------------------------
// Screen management
void InitScreen(ScreenState initialScreen);
void SetNextScreen(ScreenState state);
bool ScreenShouldClose(void);
void UpdateScreen(void);

// Menu screen
void InitMenuScreen(void);
void UpdateMenuScreen(float dt);
void RenderMenuScreen(void);

// Game screen
void InitGameScreen(void);
void UpdateGameScreen(float dt);
void RenderGameScreen(void);
void ResetBall(void);
float KeyboardInput(void);

// Collision detection
bool ResolveCollBallPaddle(Entity paddle, Vector2 ballVel);
void CalculateBouncePoints(void);
bool RayIntersectLine(Vector2 rayOrigin, Vector2 rayDir, Vector2 lineStart,
                      Vector2 lineEnd, Vector2 *collPoint, float *collTime);
float Vector2CrossProduct(Vector2 v1, Vector2 v2);
bool AABBCheck(Rectangle rect1, Rectangle rect2);
Rectangle SweptRectangle(Rectangle rect, Vector2 vel);
CollisionData SweptAABB(Rectangle rect, Vector2 vel, Rectangle target);

// -----------------------------------------------------------------------------
// Entrypoint
// -----------------------------------------------------------------------------
int main(void) {
    // pre configuration
    SetTraceLogLevel(LOG_DEBUG);

    // initialization
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_TITLE);
    InitScreen(SCREEN_MENU);

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
    CloseWindow();

    return 0;
}

// -----------------------------------------------------------------------------
// Module implementation
// -----------------------------------------------------------------------------
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
    screenFadeTimer = 0.0f;
}

void SetNextScreen(ScreenState screen) {
    screens[currentScreen].hasFinished = true;
    nextScreen = screen;
}

bool ScreenShouldClose(void) {
    return screens[currentScreen].hasFinished && nextScreen == SCREEN_NONE;
}

void UpdateScreen(void) {
    static bool isFading = false;

    float dt = GetFrameTime();

    // update screen
    if (!isFading) {
        screens[currentScreen].update(dt);
    } else {
        screenFadeTimer += dt;
    }

    // render game
    BeginDrawing();
    ClearBackground(BLACK);
    screens[currentScreen].render();
    EndDrawing();

    if (screens[currentScreen].hasFinished && nextScreen != SCREEN_NONE) {
        isFading = true;
    }

    if (isFading && screenFadeTimer > SCREEN_FADE_TIME) {
        // reset previous
        screens[currentScreen].hasFinished = false;
        isFading = false;
        screenFadeTimer = 0.0f;

        // start new
        currentScreen = nextScreen;
        nextScreen = SCREEN_NONE;
        screens[currentScreen].init();
    }
}

void InitMenuScreen(void) { TraceLog(LOG_DEBUG, "Init menu screen"); }

void UpdateMenuScreen(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetNextScreen(SCREEN_NONE);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        SetNextScreen(SCREEN_GAME);
    }
}

void RenderMenuScreen(void) {
    DrawText("PONG", 100, 100, 120,
             Fade(COLOR_FG, 1.0f - screenFadeTimer / SCREEN_FADE_TIME));
}

void InitGameScreen(void) {
    debugMode = false;
    leftScore = 0;
    rightScore = 0;

    // initialize globals
    leftPaddle = (Entity){.rect = (Rectangle){0, 0, PADDLE_WIDTH, PADDLE_HEIGHT},
                          .dir = (Vector2){0},
                          .speed = PADDLE_IA_SPEED};
    rightPaddle = leftPaddle;
    ball = (Entity){.rect = (Rectangle){0, 0, BALL_WIDTH, BALL_HEIGHT},
                    .dir = (Vector2){0},
                    .speed = BALL_INITIAL_SPEED};

    // reset paddle positions
    leftPaddle.rect.x = PADDLE_HOR_OFFSET;
    leftPaddle.rect.y = (SCREEN_HEIGHT - leftPaddle.rect.height) / 2.0f;
    rightPaddle.rect.x = SCREEN_WIDTH - PADDLE_HOR_OFFSET - rightPaddle.rect.width;
    rightPaddle.rect.y = leftPaddle.rect.y;
    rightPaddle.speed = PADDLE_SPEED;

    // IA
    topSP = (Vector2){LIMIT_LEFT + PADDLE_WIDTH, LIMIT_TOP};
    topEP = (Vector2){LIMIT_RIGHT - PADDLE_WIDTH - BALL_WIDTH, LIMIT_TOP};
    rightSP = (Vector2){LIMIT_RIGHT - PADDLE_WIDTH - BALL_WIDTH, LIMIT_TOP};
    rightEP =
        (Vector2){LIMIT_RIGHT - PADDLE_WIDTH - BALL_WIDTH, LIMIT_BOTTOM - BALL_HEIGHT};
    bottomSP = (Vector2){LIMIT_LEFT + PADDLE_WIDTH, LIMIT_BOTTOM - BALL_HEIGHT};
    bottomEP =
        (Vector2){LIMIT_RIGHT - PADDLE_WIDTH - BALL_WIDTH, LIMIT_BOTTOM - BALL_HEIGHT};
    leftSP = (Vector2){LIMIT_LEFT + PADDLE_WIDTH, LIMIT_TOP};
    leftEP = (Vector2){LIMIT_LEFT + PADDLE_WIDTH, LIMIT_BOTTOM};

    iaTargetPos = leftPaddle.rect.y;
    iaHitPos = PADDLE_HEIGHT / 2.0f;
    iaResponseTime = 0.5f;
    iaTimer = 0.0f;

    ResetBall();
}

void ResetBall(void) {
    hitCounter = 0;
    ball.speed = BALL_INITIAL_SPEED;

    ball.rect.x = (SCREEN_WIDTH - ball.rect.width) / 2.0f;
    ball.rect.y = (SCREEN_HEIGHT - ball.rect.height) / 2.0f;
    ball.dir.x = GetRandomValue(0, 1) == 0 ? -1.0f : 1.0f;
    ball.dir.y = GetRandomValue(0, 1000) / 1000.0f;
    ball.dir = Vector2Normalize(ball.dir);

    if (ball.dir.x < 0.0f) {
        CalculateBouncePoints();
        iaTargetPos = bouncePoints[bouncePointsCount].y;
    }
}

void UpdateGameScreen(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetNextScreen(SCREEN_MENU);
    }
    if (IsKeyPressed(KEY_D)) {
        debugMode = !debugMode;
    }

    // get input
    float input = KeyboardInput();
    rightPaddle.dir.y = input;

    // update paddles
    rightPaddle.rect.y += rightPaddle.dir.y * rightPaddle.speed * dt;

    // ia paddle
    iaTimer += dt;
    float leftPaddleY = leftPaddle.rect.y + iaHitPos;
    float leftPaddlePosDiff = (iaTargetPos - leftPaddleY > 0.0f) ? 1.0f : -1.0f;
    float leftPaddleFuturePos = leftPaddle.speed * dt;

    if (iaTimer > iaResponseTime) {
        if (leftPaddlePosDiff > 0.0f &&
            leftPaddleY + leftPaddleFuturePos > iaTargetPos) {
            leftPaddle.rect.y = iaTargetPos - iaHitPos;
        } else if (leftPaddlePosDiff < 0.0f &&
                   leftPaddleY - leftPaddleFuturePos < iaTargetPos) {
            leftPaddle.rect.y = iaTargetPos - iaHitPos;
        } else {
            leftPaddle.rect.y += leftPaddlePosDiff * leftPaddleFuturePos;
        }
    }

    // keep paddles on screen
    if (rightPaddle.rect.y < LIMIT_TOP) {
        rightPaddle.rect.y = LIMIT_TOP;
    } else if (rightPaddle.rect.y + rightPaddle.rect.height > LIMIT_BOTTOM) {
        rightPaddle.rect.y = LIMIT_BOTTOM - rightPaddle.rect.height;
    }
    if (leftPaddle.rect.y < LIMIT_TOP) {
        leftPaddle.rect.y = LIMIT_TOP;
    } else if (leftPaddle.rect.y + leftPaddle.rect.height > LIMIT_BOTTOM) {
        leftPaddle.rect.y = LIMIT_BOTTOM - leftPaddle.rect.height;
    }

    // update ball
    Vector2 ballVel = Vector2Scale(ball.dir, ball.speed * dt);
    bool hitLeftPaddle, hitRightPaddle;

    hitLeftPaddle = ResolveCollBallPaddle(leftPaddle, ballVel);
    hitRightPaddle = ResolveCollBallPaddle(rightPaddle, ballVel);

    if (!hitLeftPaddle && !hitRightPaddle) {
        ball.rect.x += ballVel.x;
        ball.rect.y += ballVel.y;
    } else {
        CalculateBouncePoints();
        if (hitRightPaddle) {
            iaTargetPos = bouncePoints[bouncePointsCount].y;
            iaHitPos = GetRandomValue(0, 1000) / 1000.0f * PADDLE_HEIGHT;
        } else {
            // ia hit the ball
            iaTargetPos = GetRandomValue(0, SCREEN_HEIGHT);
            iaHitPos = 0.0f;
        }

        // speed up ball
        ball.speed = BALL_INITIAL_SPEED + BALL_SPEED_INCREMENT * sqrtf(++hitCounter);

        // reset timer for ia
        iaTimer = 0.0f;
    }

    // reflect ball screen border
    if (ball.rect.y < LIMIT_TOP) {
        ball.rect.y = LIMIT_TOP;
        ball.dir.y *= -1.0f;
    } else if (ball.rect.y + ball.rect.height > LIMIT_BOTTOM) {
        ball.rect.y = LIMIT_BOTTOM - ball.rect.height;
        ball.dir.y *= -1.0f;
    }

    if (ball.rect.x + ball.rect.width < 0.0f) {
        ResetBall();
        ++rightScore;
        TraceLog(LOG_DEBUG, "Score: %dx%d", leftScore, rightScore);
    } else if (ball.rect.x > SCREEN_WIDTH) {
        ResetBall();
        ++leftScore;
        TraceLog(LOG_DEBUG, "Score: %dx%d", leftScore, rightScore);
    }

    // Check game over
    if (leftScore > 9 || rightScore > 9) {
        TraceLog(LOG_DEBUG, "Game over");
        SetNextScreen(SCREEN_MENU);
    }
}

float KeyboardInput(void) {
    float input = 0.0f;

    if (IsKeyDown(KEY_UP)) {
        input -= 1.0f;
    }
    if (IsKeyDown(KEY_DOWN)) {
        input += 1.0f;
    }

    return input;
}

void RenderGameScreen(void) {
    static Color fadeColor;
    fadeColor = Fade(COLOR_FG, 1.0f - screenFadeTimer / SCREEN_FADE_TIME);

    // draw borders
    DrawRectangle(0, 0, SCREEN_WIDTH, BORDER_WIDTH, fadeColor);
    DrawRectangle(0, SCREEN_HEIGHT - BORDER_WIDTH, SCREEN_WIDTH, BORDER_WIDTH,
                  fadeColor);

    DrawRectangleRec(leftPaddle.rect, fadeColor);
    DrawRectangleRec(rightPaddle.rect, fadeColor);
    DrawRectangleRec(ball.rect, fadeColor);

    // middle line
    int xMiddle = (SCREEN_WIDTH - BALL_WIDTH) / 2.0f;
    for (int y = 2 * BORDER_WIDTH; y < SCREEN_HEIGHT; y += 2 * BALL_HEIGHT) {
        DrawRectangle(xMiddle, y, BALL_WIDTH, BALL_HEIGHT, fadeColor);
    }

    // Draw score
    int fontSize = 90;
    const char *leftScoreText = TextFormat("%d", leftScore);
    const char *rightScoreText = TextFormat("%d", rightScore);
    int leftTextSize = MeasureText(leftScoreText, fontSize);
    int rightTextSize = MeasureText(rightScoreText, fontSize);

    DrawText(leftScoreText, 3.0f * SCREEN_WIDTH / 8.0f - leftTextSize / 2.0f, 50,
             fontSize, fadeColor);
    DrawText(rightScoreText, 5.0f * SCREEN_WIDTH / 8.0f - rightTextSize / 2.0f, 50,
             fontSize, fadeColor);

    if (debugMode) {
        // bounce points
        DrawRectangleV(bouncePoints[0], (Vector2){BALL_WIDTH, BALL_HEIGHT}, GREEN);
        for (int i = 1; i <= bouncePointsCount && i < BOUNCE_POINTS_MAX; ++i) {
            DrawRectangleV(bouncePoints[i], (Vector2){BALL_WIDTH, BALL_HEIGHT}, GREEN);
            DrawLineV(bouncePoints[i - 1], bouncePoints[i], GREEN);
        }

        DrawLineEx(topSP, topEP, 2.0f, BLUE);
        DrawLineEx(rightSP, rightEP, 2.0f, BLUE);
        DrawLineEx(bottomSP, bottomEP, 2.0f, BLUE);
        DrawLineEx(leftSP, leftEP, 2.0f, BLUE);
    }
}

bool ResolveCollBallPaddle(Entity paddle, Vector2 ballVel) {
    Rectangle ballRectSwept = SweptRectangle(ball.rect, ballVel);
    CollisionData collData = {0};

    if (AABBCheck(ballRectSwept, paddle.rect)) {
        collData = SweptAABB(ball.rect, ballVel, paddle.rect);
        if (collData.hit) {
            ball.rect.x = collData.contactPoint.x;
            ball.rect.y = collData.contactPoint.y;

            if (collData.contactNormal.x == 0) {
                // colided from top or bottom
                ball.dir.y *= -1.0f;
            } else {
                // collided from the front
                ball.dir.x *= -1.0f;
                ball.dir.y = (2 * (ball.rect.y - paddle.rect.y + ball.rect.height) /
                              (PADDLE_HEIGHT + ball.rect.height)) -
                             1.0f;
                ball.dir = Vector2Normalize(ball.dir);
            }
        }
    }

    return collData.hit;
}

void CalculateBouncePoints(void) {
    Vector2 curDir, hitPoint;
    bool hitTop, hitRight, hitBottom, hitLeft;
    float hitTime;

    // the first point is where the ball is
    bouncePointsCount = 0;
    bouncePoints[0] = (Vector2){ball.rect.x, ball.rect.y};
    curDir = ball.dir;

    while (bouncePointsCount < BOUNCE_POINTS_MAX) {
        // check top bounce
        if (curDir.y < 0.0f) {
            hitTop = RayIntersectLine(bouncePoints[bouncePointsCount], curDir, topSP,
                                      topEP, &hitPoint, &hitTime);
            if (hitTop) {
                curDir = Vector2Reflect(curDir, (Vector2){0.0f, 1.0f});
                bouncePoints[++bouncePointsCount] = hitPoint;
                continue;
            }
        }

        if (curDir.x > 0.0f) {
            hitRight = RayIntersectLine(bouncePoints[bouncePointsCount], curDir,
                                        rightSP, rightEP, &hitPoint, &hitTime);
            if (hitRight) {
                curDir = Vector2Reflect(curDir, (Vector2){-1.0f, 0.0f});
                bouncePoints[++bouncePointsCount] = hitPoint;
                break;
            }
        }

        // check bottom bounce
        if (curDir.y > 0.0f) {
            hitBottom = RayIntersectLine(bouncePoints[bouncePointsCount], curDir,
                                         bottomSP, bottomEP, &hitPoint, &hitTime);
            if (hitBottom) {
                curDir = Vector2Reflect(curDir, (Vector2){0.0f, -1.0f});
                bouncePoints[++bouncePointsCount] = hitPoint;
                continue;
            }
        }

        if (curDir.x < 0.0f) {
            hitLeft = RayIntersectLine(bouncePoints[bouncePointsCount], curDir, leftSP,
                                       leftEP, &hitPoint, &hitTime);
            if (hitLeft) {
                curDir = Vector2Reflect(curDir, (Vector2){1.0f, 0.0f});
                bouncePoints[++bouncePointsCount] = hitPoint;
                break;
            }
        }

        // we can break if don't reach any of the above
        break;
    }
}

bool RayIntersectLine(Vector2 rayOrigin, Vector2 rayDir, Vector2 lineStart,
                      Vector2 lineEnd, Vector2 *collPoint, float *collTime) {
    Vector2 a = rayOrigin, r = rayDir;
    Vector2 c = lineStart, s = Vector2Subtract(lineEnd, lineStart);

    float rCrossS = Vector2CrossProduct(r, s);
    if (FloatEquals(rCrossS, 0.0f)) {
        return false;
    }

    float t1 = Vector2CrossProduct(Vector2Subtract(c, a), s) / rCrossS;
    float t2 = Vector2CrossProduct(Vector2Subtract(c, a), r) / rCrossS;

    if (t1 >= 0.0f && (0.0f <= t2 && t2 <= 1.0f)) {
        *collPoint = Vector2Add(a, Vector2Scale(r, t1));
        *collTime = t1;
        return true;
    }

    return false;
}

float Vector2CrossProduct(Vector2 v1, Vector2 v2) { return v1.x * v2.y - v1.y * v2.x; }

bool AABBCheck(Rectangle rect1, Rectangle rect2) {
    return !(rect1.x + rect1.width < rect2.x || rect1.x > rect2.x + rect2.width ||
             rect1.y + rect1.height < rect2.y || rect1.y > rect2.y + rect2.height);
}

Rectangle SweptRectangle(Rectangle rect, Vector2 vel) {
    Rectangle sweptRect = {
        .x = vel.x > 0.0f ? rect.x : rect.x + vel.x,
        .y = vel.y > 0.0f ? rect.y : rect.y + vel.y,
        .width = vel.x > 0.0f ? rect.width + vel.x : rect.width - vel.x,
        .height = vel.y > 0.0f ? rect.height + vel.y : rect.height - vel.y};

    return sweptRect;
}

CollisionData SweptAABB(Rectangle rect, Vector2 vel, Rectangle target) {
    CollisionData data;
    Vector2 invEntry, entry, invExit, exit;
    float entryTime, exitTime;

    // initialize data with no collision
    data.hit = false;
    data.time = 1.0f;
    data.contactPoint = Vector2Zero();
    data.contactNormal = Vector2Zero();

    // find the distance between the objects on the near and far sides for both
    // x and y
    if (vel.x > 0.0f) {
        invEntry.x = target.x - (rect.x + rect.width);
        invExit.x = (target.x + target.width) - rect.x;
    } else {
        invEntry.x = (target.x + target.width) - rect.x;
        invExit.x = target.x - (rect.x + rect.width);
    }

    if (vel.y > 0.0f) {
        invEntry.y = target.y - (rect.y + rect.height);
        invExit.y = (target.y + target.height) - rect.y;
    } else {
        invEntry.y = (target.y + target.height) - rect.y;
        invExit.y = target.y - (rect.y + rect.height);
    }

    // find time of collision and time of leaving for each axis
    entry = (Vector2){-INFINITY, -INFINITY};
    exit = (Vector2){INFINITY, INFINITY};

    if (vel.x != 0) {
        entry.x = invEntry.x / vel.x;
        exit.x = invExit.x / vel.x;
    }

    if (vel.y != 0) {
        entry.y = invEntry.y / vel.y;
        entry.y = invEntry.y / vel.y;
    }

    entryTime = fmaxf(entry.x, entry.y);
    exitTime = fminf(exit.x, exit.y);

    if (entryTime > exitTime || (entry.x < 0.0f && entry.y < 0.0f) || entry.x > 1.0f ||
        entry.y > 1.0f) {
        // no collision
        return data;
    }

    // calculate normal
    if (entry.x > entry.y) {
        data.contactNormal.x = invEntry.x < 0.0f ? 1.0f : -1.0f;
    } else {
        data.contactNormal.y = invEntry.y < 0.0f ? 1.0f : -1.0f;
    }

    // calculate contact point
    data.contactPoint.x = rect.x + vel.x * entryTime;
    data.contactPoint.y = rect.y + vel.y * entryTime;

    data.hit = true;
    data.time = entryTime;

    return data;
}
