#include <math.h>
#include <raylib.h>
#include <raymath.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#define SCREEN_TITLE "Pong"
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

typedef struct CollisionData {
    bool hit;
    float time;
    Vector2 contactPoint;
    Vector2 contactNormal;
} CollisionData;

Rectangle paddleRect, ballRect;
Vector2 ballVel;
float ballSpeed;

void Init(void);
void UpdateDrawFrame(void);
bool AABBCheck(Rectangle rect1, Rectangle rect2);
Rectangle SweptRectangle(Rectangle rect, Vector2 vel);
CollisionData SweptAABB(Rectangle rect, Vector2 vel, Rectangle target);

int main(void) 
{
    // pre configuration
    SetTraceLogLevel(LOG_ERROR);
    
    // initialization
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_TITLE);
    Init();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    // pos configuration, must happen after window creation
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // gameloop
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    // cleanup
    CloseWindow();

    return 0;
}

void Init(void)
{
    paddleRect = (Rectangle) {0, 0, 150, 50};
    ballRect = (Rectangle) {0, 0, 30, 30};

    paddleRect.x = (SCREEN_WIDTH - paddleRect.width)/2.0f;
    paddleRect.y = (SCREEN_HEIGHT - paddleRect.height)/2.0f;
    ballRect.x = (SCREEN_WIDTH - ballRect.width)/2.0f;

    ballSpeed = 200;
}

void UpdateDrawFrame(void)
{
    Vector2 input, ballNormalVel;
    Rectangle ballSweptRect;
    CollisionData collData;
    float dt;
    bool isColliding = false;

    dt = GetFrameTime();

    input = Vector2Zero();
    if (IsKeyDown(KEY_LEFT)) {
        input.x -= 1.0f;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        input.x += 1.0f;
    }
    if (IsKeyDown(KEY_UP)) {
        input.y -= 1.0f;
    }
    if (IsKeyDown(KEY_DOWN)) {
        input.y += 1.0f;
    }
    input = Vector2Normalize(input);
    ballVel = Vector2Scale(input, ballSpeed);
    ballNormalVel = Vector2Scale(ballVel, dt);

    ballSweptRect = SweptRectangle(ballRect, ballNormalVel);

    if (AABBCheck(ballSweptRect, paddleRect)) {
        collData = SweptAABB(ballRect, ballNormalVel, paddleRect);
        if (collData.hit) {
            // resolve collision
            isColliding = true;
            ballRect.x = collData.contactPoint.x;
            ballRect.y = collData.contactPoint.y;

            float remainingTime = 1.0f - collData.time;
            float dotprod = (ballNormalVel.x * collData.contactNormal.y + ballNormalVel.y * collData.contactNormal.x) * remainingTime; 
            ballNormalVel.x = dotprod * collData.contactNormal.y; ballNormalVel.y = dotprod * collData.contactNormal.x;
        }
    }

    ballRect.x += ballNormalVel.x;
    ballRect.y += ballNormalVel.y;

    // drawing
    BeginDrawing();
    ClearBackground(DARKGRAY);

    Color drawColor = isColliding ? YELLOW : WHITE;
    DrawRectangleLinesEx(paddleRect, 2, drawColor);
    DrawRectangleLinesEx(ballRect, 2, drawColor);

    if (isColliding) {
        // draw normal and contact point
        Vector2 normalEnd = Vector2Add(collData.contactPoint, Vector2Scale(collData.contactNormal, 100));
        DrawCircleV(collData.contactPoint, 5, GREEN);
        DrawLineV(collData.contactPoint, normalEnd, GREEN);
    }

    EndDrawing();
}

bool AABBCheck(Rectangle rect1, Rectangle rect2)
{
    return !(rect1.x + rect1.width < rect2.x || rect1.x > rect2.x + rect2.width 
             || rect1.y + rect1.height < rect2.y || rect1.y > rect2.y + rect2.height);
}

Rectangle SweptRectangle(Rectangle rect, Vector2 vel)
{
    Rectangle sweptRect = {
        .x = vel.x > 0.0f ? rect.x : rect.x + vel.x,
        .y = vel.y > 0.0f ? rect.y : rect.y + vel.y,
        .width = vel.x > 0.0f ? rect.width + vel.x : rect.width - vel.x,
        .height = vel.y > 0.0f ? rect.height + vel.y : rect.height - vel.y
    };

    return sweptRect;
}

CollisionData SweptAABB(Rectangle rect, Vector2 vel, Rectangle target)
{
    CollisionData data;
    Vector2 invEntry, entry, invExit, exit;
    float entryTime, exitTime;

    // initialize data with no collision
    data.hit = false;
    data.time = 1.0f;
    data.contactPoint = Vector2Zero();
    data.contactNormal = Vector2Zero();

    // find the distance between the objects on the near and far sides for both x and y
    if (vel.x > 0.0f) {
        invEntry.x = target.x - (rect.x + rect.width);
        invExit.x = (target.x + target.width) - rect.x;
    }
    else {
        invEntry.x = (target.x + target.width) - rect.x;
        invExit.x = target.x - (rect.x + rect.width);
    }

    if (vel.y > 0.0f) {
        invEntry.y = target.y - (rect.y + rect.height);
        invExit.y = (target.y + target.height) - rect.y;
    }
    else {
        invEntry.y = (target.y + target.height) - rect.y;
        invExit.y = target.y - (rect.y + rect.height);
    }

    // find time of collision and time of leaving for each axis
    entry = (Vector2) { -INFINITY, -INFINITY };
    exit = (Vector2) { INFINITY, INFINITY };

    if (vel.x != 0) {
        entry.x = invEntry.x/vel.x;
        exit.x = invExit.x/vel.x;
    }

    if (vel.y != 0) {
        entry.y = invEntry.y/vel.y;
        entry.y = invEntry.y/vel.y;
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
    } 
    else { 
        data.contactNormal.y = invEntry.y < 0.0f ? 1.0f : -1.0f;
    }

    // calculate contact point
    data.contactPoint.x = rect.x + vel.x*entryTime;
    data.contactPoint.y = rect.y + vel.y*entryTime;

    data.hit = true;
    data.time = entryTime;

    return data;
}
