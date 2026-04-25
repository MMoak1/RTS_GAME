#include "ai.h"
#include "game.h"

void ai_tick(GameState* state, Team team) {
    if (state->players[team].defeated) return;

    // Throttle decisions to 1 per second (simulated APM)
    if ((state->tick_counter + team * 30) % 60 != 0) return;

    int factory_count = 0;
    int generator_count = 0;
    int troop_count = 0;
    
    Vec2 my_hq_pos = {0, 0};
    Vec2 enemy_hq_pos = {0, 0};
    bool found_enemy = false;

    // Analyze State
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active) continue;
        
        if (state->units[i].team == team) {
            if (state->units[i].type == TYPE_FACTORY) factory_count++;
            else if (state->units[i].type == TYPE_GENERATOR) generator_count++;
            else if (state->units[i].type == TYPE_HQ) {
                my_hq_pos = state->units[i].position;
            } else if (state->units[i].type < TYPE_HQ) {
                troop_count++;
            }
        } else {
            if (state->units[i].type == TYPE_HQ) {
                enemy_hq_pos = state->units[i].position;
                found_enemy = true;
            }
        }
    }

    // Economy Strategy
    if (state->players[team].points >= 50.0f && generator_count < 3) {
        float px = my_hq_pos.x + 40.0f;
        float py = my_hq_pos.y - 40.0f + (generator_count * 40.0f);
        if (game_spawn_unit(state, team, TYPE_GENERATOR, px, py)) {
            state->players[team].points -= 50.0f;
        }
    } else if (state->players[team].points >= 100.0f) {
        float px = my_hq_pos.x - 40.0f;
        float py = my_hq_pos.y - 40.0f + (factory_count * 40.0f);
        if (game_spawn_unit(state, team, TYPE_FACTORY, px, py)) {
            state->players[team].points -= 100.0f;
        }
    }

    // Swarm Strategy - wave attack every 15 units
    if (troop_count >= 15 && found_enemy) {
        for (int i = 0; i < MAX_UNITS; i++) {
            if (state->units[i].active && state->units[i].team == team && state->units[i].type < TYPE_HQ) {
                if (state->units[i].state != UNIT_ATTACKING) { // Don't interrupt them if they're focused
                    state->units[i].state = UNIT_MOVING;
                    
                    // Add slight random spread to movement
                    state->units[i].target_pos.x = enemy_hq_pos.x + ((i % 5) - 2) * 15.0f;
                    state->units[i].target_pos.y = enemy_hq_pos.y + (((i/5) % 5) - 2) * 15.0f;
                }
            }
        }
    }
}
