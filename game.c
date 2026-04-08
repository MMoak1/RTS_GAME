#include "game.h"
#include <string.h>
#include <math.h>

void grid_clear(GameState* state) {
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            state->grid.head[x][y] = -1;
        }
    }
}

void grid_build(GameState* state) {
    grid_clear(state);
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active) continue;
        
        int gx = (int)(state->units[i].position.x / CELL_SIZE);
        int gy = (int)(state->units[i].position.y / CELL_SIZE);
        
        if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT) {
            state->grid.next[i] = state->grid.head[gx][gy];
            state->grid.head[gx][gy] = i;
        } else {
            state->grid.next[i] = -1;
        }
    }
}

#define UNIT_SIZE 10.0f

void resolve_collisions(GameState* state) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active) continue;

        float ux = state->units[i].position.x;
        float uy = state->units[i].position.y;
        
        int gx = (int)(ux / CELL_SIZE);
        int gy = (int)(uy / CELL_SIZE);

        float push_x = 0;
        float push_y = 0;

        for (int cx = gx - 1; cx <= gx + 1; cx++) {
            if (cx < 0 || cx >= GRID_WIDTH) continue;
            for (int cy = gy - 1; cy <= gy + 1; cy++) {
                if (cy < 0 || cy >= GRID_HEIGHT) continue;

                int j = state->grid.head[cx][cy];
                while (j != -1) {
                    if (i != j && state->units[j].active) {
                        float dx = ux - state->units[j].position.x;
                        float dy = uy - state->units[j].position.y;
                        
                        if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f) {
                            dx = (i > j) ? 0.1f : -0.1f;
                            dy = (i > j) ? 0.1f : -0.1f;
                        }

                        if (fabsf(dx) < UNIT_SIZE && fabsf(dy) < UNIT_SIZE) {
                            float dist_sq = dx * dx + dy * dy;
                            if (dist_sq < UNIT_SIZE * UNIT_SIZE) {
                                float dist = sqrtf(dist_sq);
                                float overlap = UNIT_SIZE - dist;
                                push_x += (dx / dist) * overlap * 0.5f;
                                push_y += (dy / dist) * overlap * 0.5f;
                            }
                        }
                    }
                    j = state->grid.next[j];
                }
            }
        }
        
        state->units[i].position.x += push_x;
        state->units[i].position.y += push_y;
    }
}

void tick_medics(GameState* state) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active || state->units[i].type != TYPE_MEDIC) continue;

        float ux = state->units[i].position.x;
        float uy = state->units[i].position.y;
        int gx = (int)(ux / CELL_SIZE);
        int gy = (int)(uy / CELL_SIZE);
        bool healed_someone = false;

        for (int cx = gx - 1; cx <= gx + 1; cx++) {
            if (cx < 0 || cx >= GRID_WIDTH) continue;
            for (int cy = gy - 1; cy <= gy + 1; cy++) {
                if (cy < 0 || cy >= GRID_HEIGHT) continue;

                int j = state->grid.head[cx][cy];
                while (j != -1) {
                    if (i != j && state->units[j].active && state->units[j].team == state->units[i].team) {
                        float dx = ux - state->units[j].position.x;
                        float dy = uy - state->units[j].position.y;
                        if (dx*dx + dy*dy < 50.0f * 50.0f) {
                            if (state->units[j].health < state->units[j].max_health) {
                                state->units[j].health += 0.5f;
                                if (state->units[j].health > state->units[j].max_health)
                                    state->units[j].health = state->units[j].max_health;
                                healed_someone = true;
                            }
                        }
                    }
                    j = state->grid.next[j];
                }
            }
        }

        if (!healed_someone && state->units[i].state != UNIT_MOVING) {
            if (state->tick_counter % 30 == (uint32_t)(i % 30)) { 
                float closest_dist = 999999.0f;
                int closest_j = -1;
                for (int j = 0; j < MAX_UNITS; j++) {
                    if (i != j && state->units[j].active && state->units[j].team == state->units[i].team) {
                        float dx = ux - state->units[j].position.x;
                        float dy = uy - state->units[j].position.y;
                        float dsq = dx*dx + dy*dy;
                        if (state->units[j].health < state->units[j].max_health && dsq < closest_dist) {
                            closest_dist = dsq;
                            closest_j = j;
                        }
                    }
                }
                
                if (closest_j != -1) {
                    state->units[i].target_pos = state->units[closest_j].position;
                    state->units[i].state = UNIT_MOVING;
                }
            }
        }
    }
}

void tick_bruisers(GameState* state) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active || state->units[i].type != TYPE_BRUISER) continue;
        
        // If explicitly commanded recently by human, we don't override 
        // For now, let's always gravitate if they are idle
        if (state->units[i].state == UNIT_MOVING) continue; 

        float ux = state->units[i].position.x;
        float uy = state->units[i].position.y;
        int gx = (int)(ux / CELL_SIZE);
        int gy = (int)(uy / CELL_SIZE);
        int count = 0;
        float sum_x = 0;
        float sum_y = 0;

        for (int cx = gx - 1; cx <= gx + 1; cx++) {
            if (cx < 0 || cx >= GRID_WIDTH) continue;
            for (int cy = gy - 1; cy <= gy + 1; cy++) {
                if (cy < 0 || cy >= GRID_HEIGHT) continue;

                int j = state->grid.head[cx][cy];
                while (j != -1) {
                    if (i != j && state->units[j].active && state->units[j].team == state->units[i].team) {
                         sum_x += state->units[j].position.x;
                         sum_y += state->units[j].position.y;
                         count++;
                    }
                    j = state->grid.next[j];
                }
            }
        }

        if (count > 0) {
            float avg_x = sum_x / count;
            float avg_y = sum_y / count;
            float dx = avg_x - ux;
            float dy = avg_y - uy;
            if (dx*dx + dy*dy > 30.0f * 30.0f) {
                state->units[i].target_pos.x = avg_x;
                state->units[i].target_pos.y = avg_y;
                state->units[i].state = UNIT_MOVING;
            }
        }
    }
}

void game_init(GameState* state) {
    memset(state, 0, sizeof(GameState));
    grid_clear(state);
    
    // Spawn some initial test units
    for(int i = 0; i < 100; i++) {
        state->units[i].active = true;
        state->units[i].selected = false;
        state->units[i].team = TEAM_BLUE;
        
        if (i % 10 == 0) state->units[i].type = TYPE_MEDIC;
        else if (i % 5 == 0) state->units[i].type = TYPE_BRUISER;
        else state->units[i].type = TYPE_BASE;
        state->units[i].position.x = 100.0f + (i % 10) * 15.0f;
        state->units[i].position.y = 100.0f + (i / 10) * 15.0f;
        state->units[i].health = 100.0f;
        state->units[i].max_health = 100.0f;
        state->units[i].state = UNIT_IDLE;
    }
    
    for(int i = 0; i < 100; i++) {
        int idx = 100 + i; // Offset to avoid overwriting blue
        state->units[idx].active = true;
        state->units[idx].selected = false;
        state->units[idx].team = TEAM_RED;
        
        if (i % 10 == 0) state->units[idx].type = TYPE_MEDIC;
        else if (i % 5 == 0) state->units[idx].type = TYPE_BRUISER;
        else state->units[idx].type = TYPE_BASE;
        state->units[idx].position.x = 600.0f + (i % 10) * 15.0f;
        state->units[idx].position.y = 400.0f + (i / 10) * 15.0f;
        state->units[idx].health = 100.0f;
        state->units[idx].max_health = 100.0f;
        state->units[idx].state = UNIT_IDLE;
    }
    
    grid_build(state);
}

float pseudo_random_jitter(int unit_id, uint32_t tick_counter) {
    uint32_t seed = (unit_id * 193) ^ tick_counter;
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return ((seed % 2000) / 1000.0f) - 1.0f;
}

void tick_movement(GameState* state) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active) continue;
        
        float ux = state->units[i].position.x;
        float uy = state->units[i].position.y;
        
        if (state->units[i].type == TYPE_BASE) {
            if (state->units[i].state == UNIT_MOVING) {
                float force_x = 0;
                float force_y = 0;
                
                int gx = (int)(ux / CELL_SIZE);
                int gy = (int)(uy / CELL_SIZE);
                
                float align_x = 0, align_y = 0;
                float coh_x = 0, coh_y = 0;
                int boid_count = 0;
                
                for (int cx = gx - 1; cx <= gx + 1; cx++) {
                    if (cx < 0 || cx >= GRID_WIDTH) continue;
                    for (int cy = gy - 1; cy <= gy + 1; cy++) {
                        if (cy < 0 || cy >= GRID_HEIGHT) continue;
                        int j = state->grid.head[cx][cy];
                        while (j != -1) {
                            if (i != j && state->units[j].active && state->units[j].team == state->units[i].team && state->units[j].type == TYPE_BASE) {
                                float dx = ux - state->units[j].position.x;
                                float dy = uy - state->units[j].position.y;
                                float dist_sq = dx*dx + dy*dy;
                                
                                if (dist_sq < 40.0f * 40.0f) {
                                    align_x += state->units[j].velocity.x;
                                    align_y += state->units[j].velocity.y;
                                    coh_x += state->units[j].position.x;
                                    coh_y += state->units[j].position.y;
                                    boid_count++;
                                }
                            }
                            j = state->grid.next[j];
                        }
                    }
                }
                
                if (boid_count > 0) {
                    align_x /= boid_count;
                    align_y /= boid_count;
                    force_x += align_x * 0.05f;
                    force_y += align_y * 0.05f;
                    
                    coh_x /= boid_count;
                    coh_y /= boid_count;
                    float cdx = coh_x - ux;
                    float cdy = coh_y - uy;
                    float c_dist = sqrtf(cdx*cdx + cdy*cdy);
                    if (c_dist > 0.1f) {
                        force_x += (cdx / c_dist) * 0.05f;
                        force_y += (cdy / c_dist) * 0.05f;
                    }
                }

                // Add deterministic "organic" jitter
                force_x += pseudo_random_jitter(i, state->tick_counter) * 0.15f;
                force_y += pseudo_random_jitter(i + MAX_UNITS, state->tick_counter) * 0.15f;
                
                float tdx = state->units[i].target_pos.x - ux;
                float tdy = state->units[i].target_pos.y - uy;
                float tdist = sqrtf(tdx*tdx + tdy*tdy);
                
                if (tdist > UNIT_SIZE * 0.5f) {
                     force_x += (tdx / tdist) * 0.2f;
                     force_y += (tdy / tdist) * 0.2f;
                } else {
                     state->units[i].state = UNIT_IDLE;
                }
                
                state->units[i].velocity.x += force_x;
                state->units[i].velocity.y += force_y;
            }
            
            float current_speed = sqrtf(state->units[i].velocity.x * state->units[i].velocity.x + state->units[i].velocity.y * state->units[i].velocity.y);
            float MAX_SPEED = 2.0f;
            if (current_speed > MAX_SPEED) {
                state->units[i].velocity.x = (state->units[i].velocity.x / current_speed) * MAX_SPEED;
                state->units[i].velocity.y = (state->units[i].velocity.y / current_speed) * MAX_SPEED;
            }
            
            state->units[i].position.x += state->units[i].velocity.x;
            state->units[i].position.y += state->units[i].velocity.y;
            
            state->units[i].velocity.x *= 0.95f;
            state->units[i].velocity.y *= 0.95f;
            
        } else {
            if (state->units[i].state == UNIT_MOVING) {
                float dx = state->units[i].target_pos.x - ux;
                float dy = state->units[i].target_pos.y - uy;
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist > UNIT_SIZE * 0.5f) {
                    float speed = 2.0f;
                    if (state->units[i].type == TYPE_BRUISER) speed = 1.0f;
                    if (state->units[i].type == TYPE_MEDIC) speed = 2.4f;
                    
                    state->units[i].position.x += (dx / dist) * speed;
                    state->units[i].position.y += (dy / dist) * speed;
                } else {
                    state->units[i].state = UNIT_IDLE;
                }
            }
        }
    }
}

void game_tick(GameState* state) {
    state->tick_counter++;
    
    tick_medics(state);
    tick_bruisers(state);
    
    tick_movement(state);
    
    // Separation / Collision Avoidance
    resolve_collisions(state);
    
    // Rebuild spatial grid since positions changed
    grid_build(state);
}

void command_move_selected(GameState* state, float target_x, float target_y) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active || !state->units[i].selected) continue;
        
        state->units[i].state = UNIT_MOVING;
        state->units[i].target_pos.x = target_x;
        state->units[i].target_pos.y = target_y;
    }
}
