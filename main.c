#include <raylib.h>
#include <math.h>
#include "game.h"

#define FPS_LIMIT 60
#define TICK_RATE 60

GameState game_state;

int main(void) {
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Simple RTS - DOD/C");
    SetTargetFPS(FPS_LIMIT);

    game_init(&game_state);

    float tick_timer = 0.0f;
    float time_per_tick = 1.0f / TICK_RATE;

    Vector2 selection_start = {0};
    bool is_selecting = false;

    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0.0f, 0.0f };
    camera.offset = (Vector2){ 0.0f, 0.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        // Camera panning
        float cam_speed = 15.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) camera.target.x += cam_speed;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) camera.target.x -= cam_speed;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) camera.target.y -= cam_speed;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) camera.target.y += cam_speed;
        
        // Input processing translated to world coordinates
        Vector2 mouse_screen = GetMousePosition();
        Vector2 mouse_world = GetScreenToWorld2D(mouse_screen, camera);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selection_start = mouse_world;
            is_selecting = true;
        }
        
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            is_selecting = false;
            Vector2 selection_end = mouse_world;
            Rectangle rec = {
                fminf(selection_start.x, selection_end.x),
                fminf(selection_start.y, selection_end.y),
                fabsf(selection_end.x - selection_start.x),
                fabsf(selection_end.y - selection_start.y)
            };
            
            if (rec.width < 5 && rec.height < 5) {
                rec.x -= 5.0f; rec.y -= 5.0f;
                rec.width = 10.0f; rec.height = 10.0f;
            }
            
            for (int i = 0; i < MAX_UNITS; i++) {
                if (!game_state.units[i].active || game_state.units[i].team != TEAM_BLUE) continue;
                Vector2 pos = { game_state.units[i].position.x + 5, game_state.units[i].position.y + 5 }; // center
                game_state.units[i].selected = CheckCollisionPointRec(pos, rec);
            }
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            command_move_selected(&game_state, mouse_world.x, mouse_world.y);
        }

        // Logic Tick (Fixed Timestep)
        float dt = GetFrameTime();
        tick_timer += dt;
        
        while (tick_timer >= time_per_tick) {
            game_tick(&game_state);
            tick_timer -= time_per_tick;
        }

        // Rendering
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        BeginMode2D(camera);
        
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!game_state.units[i].active) continue;
            
            Color c = (game_state.units[i].team == TEAM_BLUE) ? BLUE : RED;
            int px = (int)game_state.units[i].position.x;
            int py = (int)game_state.units[i].position.y;
            
            if (game_state.units[i].type == TYPE_BRUISER) {
                DrawRectangle(px - 3, py - 3, 16, 16, c);
            } else if (game_state.units[i].type == TYPE_MEDIC) {
                DrawRectangle(px + 1, py + 1, 8, 8, c);
                DrawRectangleLines(px, py, 10, 10, WHITE);
            } else {
                DrawRectangle(px, py, 10, 10, c);
            }
            
            if (game_state.units[i].selected) {
                DrawRectangleLines(px - 2, py - 2, 14, 14, GREEN);
            }
        }

        if (is_selecting) {
            Rectangle rec = {
                fminf(selection_start.x, mouse_world.x),
                fminf(selection_start.y, mouse_world.y),
                fabsf(mouse_world.x - selection_start.x),
                fabsf(mouse_world.y - selection_start.y)
            };
            DrawRectangleLinesEx(rec, 1.0f, GREEN);
        }
        
        EndMode2D();

        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
