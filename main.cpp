#include <raylib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "game.h"

#define FPS_LIMIT 60
#define TICK_RATE 60

int main(int argc, char** argv) {
    GameState* d_state = game_alloc();
    GameState& game_state = *d_state;

    game_init(&game_state);

    game_state.headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            game_state.headless = true;
        }
    }

    if (game_state.headless) {
        printf("Running in headless mode for AI training...\n");
        int ticks = 0;
        while (true) {
            game_tick(&game_state);
            ticks++;
            
            if (game_state.players[TEAM_BLUE].defeated) {
                printf("BLUE DEFEATED after %d ticks!\n", ticks);
                break;
            }
            if (game_state.players[TEAM_RED].defeated) {
                printf("RED DEFEATED after %d ticks!\n", ticks);
                break;
            }
        }
    } else {
        // Initialization
        const int screenWidth = 800;
        const int screenHeight = 600;

        InitWindow(screenWidth, screenHeight, "Simple RTS - DOD/Nvidia");
        SetTargetFPS(FPS_LIMIT);

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
                if (!game_state.units[i].active || game_state.units[i].team != TEAM_BLUE || game_state.units[i].type >= TYPE_HQ) continue;
                Vector2 pos = { game_state.units[i].position.x + 5, game_state.units[i].position.y + 5 }; // center
                game_state.units[i].selected = CheckCollisionPointRec(pos, rec);
            }
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            command_move_selected(&game_state, mouse_world.x, mouse_world.y);
        }
        
        // Economy Hotkeys
        if (!game_state.players[TEAM_BLUE].defeated) {
            if (IsKeyPressed(KEY_F)) { // Build Factory (100 pts)
                if (game_state.players[TEAM_BLUE].points >= 100.0f) {
                    if (game_spawn_unit(&game_state, TEAM_BLUE, TYPE_FACTORY, mouse_world.x, mouse_world.y)) {
                        game_state.players[TEAM_BLUE].points -= 100.0f;
                    }
                }
            }
            if (IsKeyPressed(KEY_G)) { // Build Generator (50 pts)
                if (game_state.players[TEAM_BLUE].points >= 50.0f) {
                    if (game_spawn_unit(&game_state, TEAM_BLUE, TYPE_GENERATOR, mouse_world.x, mouse_world.y)) {
                        game_state.players[TEAM_BLUE].points -= 50.0f;
                    }
                }
            }
                 // manual troop spawn removed
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
            
            if (game_state.units[i].type == TYPE_HQ) {
                DrawRectangle(px - 15, py - 15, 40, 40, c);
                DrawRectangleLines(px - 15, py - 15, 40, 40, WHITE);
            } else if (game_state.units[i].type == TYPE_FACTORY) {
                DrawRectangle(px - 10, py - 10, 30, 30, c);
                DrawRectangleLines(px - 10, py - 10, 30, 30, YELLOW);
            } else if (game_state.units[i].type == TYPE_GENERATOR) {
                DrawRectangle(px - 10, py - 10, 30, 30, c);
                DrawRectangleLines(px - 10, py - 10, 30, 30, GREEN);
            } else if (game_state.units[i].type == TYPE_BRUISER) {
                DrawRectangle(px - 3, py - 3, 16, 16, c);
            } else if (game_state.units[i].type == TYPE_MEDIC) {
                DrawRectangle(px + 1, py + 1, 8, 8, c);
                DrawRectangleLines(px, py, 10, 10, WHITE);
            } else if (game_state.units[i].type == TYPE_AGGRESSOR) {
                DrawRectangle(px, py, 10, 10, c);
                DrawRectangleLines(px - 1, py - 1, 12, 12, ORANGE);
            } else if (game_state.units[i].type == TYPE_REPELLER) {
                DrawRectangle(px + 2, py + 2, 6, 6, c);
                DrawRectangleLines(px - 1, py - 1, 12, 12, PURPLE);
            } else {
                DrawRectangle(px, py, 10, 10, c);
            }
            
            if (game_state.units[i].attack_fx_timer > 0) {
                float lw = (game_state.units[i].type == TYPE_HQ) ? 5.0f : 1.5f;
                DrawLineEx((Vector2){game_state.units[i].position.x + 5, game_state.units[i].position.y + 5}, 
                           (Vector2){game_state.units[i].last_attack_target.x + 5, game_state.units[i].last_attack_target.y + 5}, 
                           lw, (game_state.units[i].team == TEAM_BLUE) ? SKYBLUE : PINK);
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
        
        // Draw UI
        DrawText(TextFormat("Blue Points: %d", (int)game_state.players[TEAM_BLUE].points), 10, 40, 20, BLUE);
        DrawText(TextFormat("Red Points: %d", (int)game_state.players[TEAM_RED].points), 10, 70, 20, RED);
        DrawText("Hotkeys: [F]actory (100) | [G]enerator (50)", 10, screenHeight - 30, 20, DARKGRAY);
        
        if (game_state.players[TEAM_BLUE].defeated) {
            DrawText("BLUE DEFEATED!", screenWidth/2 - 200, screenHeight/2, 50, RED);
        } else if (game_state.players[TEAM_RED].defeated) {
            DrawText("RED DEFEATED!", screenWidth/2 - 200, screenHeight/2, 50, BLUE);
        }
        
        EndDrawing();
    }

        CloseWindow();
    }

    game_free(d_state);
    return 0;
}
