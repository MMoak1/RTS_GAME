#include "game.h"
#include <string.h>

void game_init(GameState* state) {
    memset(state, 0, sizeof(GameState));
    
    // Spawn some initial test units
    for(int i = 0; i < 100; i++) {
        state->units[i].active = true;
        state->units[i].team = TEAM_BLUE;
        state->units[i].position.x = 100.0f + (i % 10) * 15.0f;
        state->units[i].position.y = 100.0f + (i / 10) * 15.0f;
        state->units[i].health = 100.0f;
        state->units[i].max_health = 100.0f;
    }
    
    for(int i = 0; i < 100; i++) {
        int idx = 100 + i; // Offset to avoid overwriting blue
        state->units[idx].active = true;
        state->units[idx].team = TEAM_RED;
        state->units[idx].position.x = 600.0f + (i % 10) * 15.0f;
        state->units[idx].position.y = 400.0f + (i / 10) * 15.0f;
        state->units[idx].health = 100.0f;
        state->units[idx].max_health = 100.0f;
    }
}

void game_tick(GameState* state) {
    state->tick_counter++;
    
    // Placeholder movement test
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active) continue;
        
        // Jitter movement just to show life and verify tick is running
        if (state->units[i].team == TEAM_BLUE) {
            state->units[i].position.x += 0.05f;
        } else {
            state->units[i].position.x -= 0.05f;
        }
    }
}
