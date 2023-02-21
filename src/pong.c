#include <math.h>
#include <raylib.h>
#include <raymath.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define SCREEN_TITLE  "Pong"
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

#define COLOR_BG      BLACK
#define COLOR_ENITIES WHITE

#define PADDLE_WIDTH      20
#define PADDLE_HEIGHT     80
#define PADDLE_SPEED      600
#define PADDLE_HOR_OFFSET 30

#define BALL_WIDTH         15
#define BALL_HEIGHT        15
#define BALL_INITIAL_SPEED 300

#define BORDER_WIDTH 20

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

static Entity leftPaddle, rightPaddle, ball;
static int topLimit, rightLimit, bottomLimit, leftLimit;

// -----------------------------------------------------------------------------
// Module declaration
// -----------------------------------------------------------------------------
// Initialization
static void ResetGame(void);
static void ResetBall(void);

// Game loop
static void GameLoop(void);
static float KeyboardInput(void);
static void DrawGame(void);

// Collision detection
static bool AABBCheck(Rectangle rect1, Rectangle rect2);
static Rectangle SweptRectangle(Rectangle rect, Vector2 vel);
static CollisionData SweptAABB(Rectangle rect, Vector2 vel, Rectangle target);

// -----------------------------------------------------------------------------
// Entrypoint
// -----------------------------------------------------------------------------
int main(void) {
    // pre configuration
    SetTraceLogLevel(LOG_DEBUG);

    // initialization
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_TITLE);
    ResetGame();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    // pos configuration, must happen after window creation
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // gameloop
    while (!WindowShouldClose()) {
        GameLoop();
    }
#endif

    // cleanup
    CloseWindow();

    return 0;
}

// -----------------------------------------------------------------------------
// Module implementation
// -----------------------------------------------------------------------------
void ResetGame(void) {
    // initialize globals
    leftPaddle =
        (Entity){.rect = (Rectangle){0, 0, PADDLE_WIDTH, PADDLE_HEIGHT},
                 .dir = (Vector2){0},
                 .speed = PADDLE_SPEED};
    rightPaddle = leftPaddle;
    ball = (Entity){.rect = (Rectangle){0, 0, BALL_WIDTH, BALL_HEIGHT},
                    .dir = (Vector2){0},
                    .speed = BALL_INITIAL_SPEED};

    // limits
    topLimit = BORDER_WIDTH;
    rightLimit = SCREEN_WIDTH - PADDLE_HOR_OFFSET;
    bottomLimit = SCREEN_HEIGHT - BORDER_WIDTH;
    leftLimit = PADDLE_HOR_OFFSET;

    // reset paddle positions
    leftPaddle.rect.x = PADDLE_HOR_OFFSET;
    leftPaddle.rect.y = (SCREEN_HEIGHT - leftPaddle.rect.height) / 2.0f;
    rightPaddle.rect.x =
        SCREEN_WIDTH - PADDLE_HOR_OFFSET - rightPaddle.rect.width;
    rightPaddle.rect.y = leftPaddle.rect.y;

    ResetBall();
}

static void ResetBall(void) {
    ball.rect.x = (SCREEN_WIDTH - ball.rect.width) / 2.0f;
    ball.rect.y = (SCREEN_HEIGHT - ball.rect.height) / 2.0f;
    ball.dir.x = GetRandomValue(0, 1) == 0 ? -1.0f : 1.0f;
    ball.dir.y = GetRandomValue(0, 1000) / 1000.0f;
    ball.dir = Vector2Normalize(ball.dir);
}

static void GameLoop(void) {
    // get input
    float input = KeyboardInput();
    rightPaddle.dir.y = input;

    // update paddles
    float dt = GetFrameTime();
    rightPaddle.rect.y += rightPaddle.dir.y * rightPaddle.speed * dt;
    leftPaddle.rect.y = ball.rect.y;

    // keep paddles on screen
    if (rightPaddle.rect.y < topLimit) {
        rightPaddle.rect.y = topLimit;
    } else if (rightPaddle.rect.y + rightPaddle.rect.height > bottomLimit) {
        rightPaddle.rect.y = bottomLimit - rightPaddle.rect.height;
    }
    if (leftPaddle.rect.y < topLimit) {
        leftPaddle.rect.y = topLimit;
    } else if (leftPaddle.rect.y + leftPaddle.rect.height > bottomLimit) {
        leftPaddle.rect.y = bottomLimit - leftPaddle.rect.height;
    }

    // update ball
    Vector2 ballVel = Vector2Scale(ball.dir, ball.speed * dt);
    Rectangle ballRectSwept = SweptRectangle(ball.rect, ballVel);
    CollisionData ballCollLeftPaddle = {0};
    CollisionData ballCollRightPaddle = {0};

    if (AABBCheck(ballRectSwept, leftPaddle.rect)) {
        ballCollLeftPaddle = SweptAABB(ball.rect, ballVel, leftPaddle.rect);
        if (ballCollLeftPaddle.hit) {
            ball.rect.x = ballCollLeftPaddle.contactPoint.x;
            ball.rect.y = ballCollLeftPaddle.contactPoint.y;

            if (ballCollLeftPaddle.contactNormal.x == 0) {
                // colided from top or bottom
                ball.dir.y *= -1.0f;
            } else {
                // collided from the front
                ball.dir.x *= -1.0f;
                ball.dir.y =
                    2 * (ball.rect.y - leftPaddle.rect.y + ball.rect.height) /
                        (PADDLE_HEIGHT + ball.rect.height) -
                    1.0f;
                ball.dir = Vector2Normalize(ball.dir);
            }
        }
    }
    if (AABBCheck(ballRectSwept, rightPaddle.rect)) {
        ballCollRightPaddle = SweptAABB(ball.rect, ballVel, rightPaddle.rect);
        if (ballCollRightPaddle.hit) {
            // resolve and reflect
            ball.rect.x = ballCollRightPaddle.contactPoint.x;
            ball.rect.y = ballCollRightPaddle.contactPoint.y;
            if (ballCollRightPaddle.contactNormal.x == 0) {
                // colided from top or bottom
                ball.dir.y *= -1.0f;
            } else {
                // collided from the front
                ball.dir.x *= -1.0f;
                ball.dir.y =
                    2 * (ball.rect.y - rightPaddle.rect.y + ball.rect.height) /
                        (PADDLE_HEIGHT + ball.rect.height) -
                    1.0f;
                ball.dir = Vector2Normalize(ball.dir);
            }
        }
    }

    if (!ballCollLeftPaddle.hit && !ballCollRightPaddle.hit) {
        ball.rect.x += ballVel.x;
        ball.rect.y += ballVel.y;
    }

    // reflect ball screen border
    if (ball.rect.y < topLimit) {
        ball.rect.y = topLimit;
        ball.dir.y *= -1.0f;
    } else if (ball.rect.y + ball.rect.height > bottomLimit) {
        ball.rect.y = bottomLimit - ball.rect.height;
        ball.dir.y *= -1.0f;
    }

    if (ball.rect.x + ball.rect.width < 0.0f || ball.rect.x > SCREEN_WIDTH) {
        ResetBall();
    }

    DrawGame();
}

static float KeyboardInput(void) {
    float input = 0.0f;

    if (IsKeyDown(KEY_UP)) {
        input -= 1.0f;
    }
    if (IsKeyDown(KEY_DOWN)) {
        input += 1.0f;
    }

    return input;
}

static void DrawGame(void) {
    BeginDrawing();
    ClearBackground(BLACK);

    // draw borders
    DrawRectangle(0, 0, SCREEN_WIDTH, BORDER_WIDTH, COLOR_ENITIES);
    DrawRectangle(0, SCREEN_HEIGHT - BORDER_WIDTH, SCREEN_WIDTH, BORDER_WIDTH,
                  COLOR_ENITIES);

    DrawRectangleRec(leftPaddle.rect, WHITE);
    DrawRectangleRec(rightPaddle.rect, WHITE);
    DrawRectangleRec(ball.rect, WHITE);

    EndDrawing();
}

static bool AABBCheck(Rectangle rect1, Rectangle rect2) {
    return !(
        rect1.x + rect1.width < rect2.x || rect1.x > rect2.x + rect2.width ||
        rect1.y + rect1.height < rect2.y || rect1.y > rect2.y + rect2.height);
}

static Rectangle SweptRectangle(Rectangle rect, Vector2 vel) {
    Rectangle sweptRect = {
        .x = vel.x > 0.0f ? rect.x : rect.x + vel.x,
        .y = vel.y > 0.0f ? rect.y : rect.y + vel.y,
        .width = vel.x > 0.0f ? rect.width + vel.x : rect.width - vel.x,
        .height = vel.y > 0.0f ? rect.height + vel.y : rect.height - vel.y};

    return sweptRect;
}

static CollisionData SweptAABB(Rectangle rect, Vector2 vel, Rectangle target) {
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

    if (entryTime > exitTime || (entry.x < 0.0f && entry.y < 0.0f) ||
        entry.x > 1.0f || entry.y > 1.0f) {
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
