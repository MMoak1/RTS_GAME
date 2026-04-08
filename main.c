#include <raylib.h>
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

    while (!WindowShouldClose()) {
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
        
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!game_state.units[i].active) continue;
            
            Color c = (game_state.units[i].team == TEAM_BLUE) ? BLUE : RED;
            DrawRectangle((int)game_state.units[i].position.x, 
                          (int)game_state.units[i].position.y, 
                          10, 10, c);
        }

        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
