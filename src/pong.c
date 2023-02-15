#include <raylib.h>
#include <raymath.h>

#define SCREEN_TITLE "Pong"
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define SWAP(x, y, T) do { T SWAP = x; x = y; y = SWAP; } while (0)

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

Rectangle obj, target;
Vector2 objVel;

void Init(void);
void UpdateDrawFrame(void);
bool SweptAABBCollision(Rectangle rect, Vector2 vel, Rectangle targetRec, float dt,
                        float *entryTime, Vector2 *contactPoint, Vector2 *contactNormal);

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
    obj = (Rectangle) { 0, 0, 50, 50 };
    obj.x = (SCREEN_WIDTH - obj.width) / 2.0f; 

    target = (Rectangle) { 0, 0, 300, 150 };
    target.x = (SCREEN_WIDTH - target.width) / 2.0f;
    target.y = (SCREEN_HEIGHT - target.height) / 2.0f;

    objVel = Vector2Zero();
}

void UpdateDrawFrame(void)
{
    Vector2 input, contactPoint, contactNormal;
    float dt, entryTime;

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
    objVel = Vector2Scale(input, 300);

    // drawing
    BeginDrawing();
    ClearBackground(DARKGRAY);
    DrawRectangleLinesEx(obj, 1.0, WHITE);
    if (SweptAABBCollision(obj, objVel, target, dt, &entryTime, &contactPoint, &contactNormal)) {
        DrawRectangleLinesEx(target, 1.0, YELLOW);
        DrawCircleV(contactPoint, 5.0f, YELLOW);
        Vector2 normalDraw = Vector2Add(contactPoint, Vector2Scale(contactNormal, 25.0f));
        DrawLineV(contactPoint, normalDraw, YELLOW);
        // resolve collision
        obj.x = contactPoint.x - obj.width/2.0f;
        obj.y = contactPoint.y - obj.height/2.0f;
        objVel = Vector2Zero();
    } else {
        DrawRectangleLinesEx(target, 1.0, WHITE);
        obj.x += objVel.x*dt;
        obj.y += objVel.y*dt;
    }
    EndDrawing();
}

bool SweptAABBCollision(Rectangle rect, Vector2 vel, Rectangle targetRec, float dt,
                        float *entryTime, Vector2 *contactPoint, Vector2 *contactNormal)
{
    Vector2 origin;
    Vector2 targetPos, targetEnd;
    Vector2 near, far;
    float exitTime;

    if (vel.x == 0.0f && vel.y == 0.0f) return false;

    // extended target pos
    origin = (Vector2) { rect.x + rect.width/2.0f, rect.y + rect.height/2.0f };
    targetPos = (Vector2) { targetRec.x - rect.width/2.0f, targetRec.y - rect.height/2.0f };
    targetEnd = (Vector2) { targetRec.x + targetRec.width + rect.width/2.0f, targetRec.y + targetRec.height + rect.height/2.0f };

    vel = Vector2Scale(vel, dt);

    near = Vector2Divide(Vector2Subtract(targetPos, origin), vel);
    far = Vector2Divide(Vector2Subtract(targetEnd, origin), vel);

    if (near.x > far.x) SWAP(near.x, far.x, float);
    if (near.y > far.y) SWAP(near.y, far.y, float);

    if (near.x > far.y || near.y > far.x) return false;

    *entryTime = MAX(near.x, near.y);
    exitTime = MIN(far.x, far.y);

    if (exitTime < 0.0f) return false;
    if (*entryTime >= 1.0f) return false;

    *contactPoint = Vector2Add(origin, Vector2Scale(vel, *entryTime));
    *contactNormal = Vector2Zero();

    if (near.x > near.y) {
        contactNormal->x = vel.x < 0.0f ? 1.0f : -1.0f;
    } else if (near.x < near.y) {
        contactNormal->y = vel.y < 0.0f ? 1.0f : -1.0f;
    }

    return true;
}
