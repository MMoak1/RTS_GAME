#include "game.h"
#include "ai.h"
#include <string.h>
#include <math.h>


/*
GameState* game_alloc() {
    GameState* state;
    cudaMallocManaged(&state, sizeof(GameState));
    return state;
}

void game_free(GameState* state) {
    cudaFree(state);
}

__global__ void kernel_grid_clear(GameState* state) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < GRID_WIDTH * GRID_HEIGHT) {
        int x = idx / GRID_HEIGHT;
        int y = idx % GRID_HEIGHT;
        state->grid.head[x][y] = -1;
    }
}

__global__ void kernel_grid_build(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS) {
        if (!state->units[i].active) {
            state->grid.next[i] = -1;
            return;
        }
        
        int gx = (int)(state->units[i].position.x / CELL_SIZE);
        int gy = (int)(state->units[i].position.y / CELL_SIZE);
        
        if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT) {
            int old_head = atomicExch(&state->grid.head[gx][gy], i);
            state->grid.next[i] = old_head;
        } else {
            state->grid.next[i] = -1;
        }
    }
}

__global__ void kernel_resolve_collisions(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS && state->units[i].active) {
        float ux = state->units[i].position.x;
        float uy = state->units[i].position.y;
        float r_i = state->units[i].radius;
        
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
                        float r_j = state->units[j].radius;
                        float dx = ux - state->units[j].position.x;
                        float dy = uy - state->units[j].position.y;
                        
                        if (fabsf(dx) < 0.001f && fabsf(dy) < 0.001f) {
                            dx = (i > j) ? 0.1f : -0.1f;
                            dy = (i > j) ? 0.1f : -0.1f;
                        }

                        float combined_r = r_i + r_j;
                        if (fabsf(dx) < combined_r && fabsf(dy) < combined_r) {
                            float dist_sq = dx * dx + dy * dy;
                            if (dist_sq < combined_r * combined_r) {
                                float dist = sqrtf(dist_sq);
                                float overlap = combined_r - dist;
                                // Heavy buildings can't be pushed much, so standard units take the brunt
                                float push_factor = (state->units[i].type >= TYPE_HQ) ? 0.1f : 0.5f;
                                push_x += (dx / dist) * overlap * push_factor;
                                push_y += (dy / dist) * overlap * push_factor;
                            }
                        }
                    }
                    j = state->grid.next[j];
                }
            }
        }
        
        // Stationary buildings shouldn't be pushed
        if (state->units[i].type < TYPE_HQ) {
            state->units[i].position.x += push_x;
            state->units[i].position.y += push_y;
        }
    }
}

__global__ void kernel_tick_medics(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS && state->units[i].active && state->units[i].type == TYPE_MEDIC) {

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
                                atomicAdd(&state->units[j].health, 0.5f);
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

__global__ void kernel_tick_bruisers(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS && state->units[i].active && state->units[i].type == TYPE_BRUISER) {
        
        // If explicitly commanded recently by human, we don't override 
        // For now, let's always gravitate if they are idle
        if (state->units[i].state == UNIT_MOVING) return; 

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

__host__ __device__ bool game_spawn_unit(GameState* state, Team team, UnitType type, float x, float y) {
    for (int i = 0; i < MAX_UNITS; i++) {
#ifndef __CUDA_ARCH__
        if (!state->units[i].active) {
            state->units[i].active = 1;
#else
        if (atomicCAS(&state->units[i].active, 0, 1) == 0) {
#endif
            state->units[i].selected = false;
            state->units[i].team = team;
            state->units[i].type = type;
            state->units[i].position.x = x;
            state->units[i].position.y = y;
            state->units[i].velocity = (Vec2){0,0};
            state->units[i].state = UNIT_IDLE;
            state->units[i].attack_cooldown = 0;
            state->units[i].attack_fx_timer = 0;
            
            if (type == TYPE_HQ) {
                state->units[i].radius = 20.0f;
                state->units[i].max_health = 5000.0f;
            } else if (type == TYPE_FACTORY || type == TYPE_GENERATOR) {
                state->units[i].radius = 15.0f;
                state->units[i].max_health = 1000.0f;
            } else if (type == TYPE_BRUISER) {
                state->units[i].radius = 8.0f;
                state->units[i].max_health = 300.0f;
            } else if (type == TYPE_MEDIC) {
                state->units[i].radius = 4.0f;
                state->units[i].max_health = 50.0f;
            } else { 
                state->units[i].radius = 5.0f;
                state->units[i].max_health = 100.0f;
            }
            state->units[i].health = state->units[i].max_health;
            if (i >= state->unit_count) state->unit_count = i + 1; // Not atomic but harmless if it's slightly off during mass spawns
            return true;
        }
    }
    return false;
}

void game_init(GameState* state) {
    memset(state, 0, sizeof(GameState));
    grid_clear(state);
    
    state->players[TEAM_BLUE].points = 150.0f;
    state->players[TEAM_RED].points = 150.0f;
    
    // Spawn Blue HQ
    game_spawn_unit(state, TEAM_BLUE, TYPE_HQ, 100.0f, 300.0f);
    game_spawn_unit(state, TEAM_BLUE, TYPE_GENERATOR, 100.0f, 200.0f);
    game_spawn_unit(state, TEAM_BLUE, TYPE_FACTORY, 100.0f, 400.0f);
    
    // Spawn some initial blue test units
    for(int i = 0; i < 20; i++) {
        UnitType type = TYPE_BASE;
        if (i % 8 == 0) type = TYPE_AGGRESSOR;
        else if (i % 7 == 0) type = TYPE_REPELLER;
        
        float x = 200.0f + (i % 5) * 15.0f;
        float y = 250.0f + (i / 5) * 15.0f;
        game_spawn_unit(state, TEAM_BLUE, type, x, y);
    }
    
    // Spawn Red HQ
    game_spawn_unit(state, TEAM_RED, TYPE_HQ, 700.0f, 300.0f);
    game_spawn_unit(state, TEAM_RED, TYPE_GENERATOR, 700.0f, 200.0f);
    game_spawn_unit(state, TEAM_RED, TYPE_FACTORY, 700.0f, 400.0f);
    
    // Spawn some red test units
    for(int i = 0; i < 20; i++) {
        UnitType type = TYPE_BASE;
        if (i % 8 == 0) type = TYPE_AGGRESSOR;
        else if (i % 7 == 0) type = TYPE_REPELLER;
        
        float x = 600.0f + (i % 5) * 15.0f;
        float y = 250.0f + (i / 5) * 15.0f;
        game_spawn_unit(state, TEAM_RED, type, x, y);
    }
    
    grid_build(state);
}

__device__ float pseudo_random_jitter(int unit_id, uint32_t tick_counter) {
    uint32_t seed = (unit_id * 193) ^ tick_counter;
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return ((seed % 2000) / 1000.0f) - 1.0f;
}

__global__ void kernel_tick_movement(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS && state->units[i].active) {
        
        float ux = state->units[i].position.x;
        float uy = state->units[i].position.y;
        
        if (state->units[i].type == TYPE_BASE || state->units[i].type == TYPE_AGGRESSOR || state->units[i].type == TYPE_REPELLER) {
            if (state->units[i].state == UNIT_MOVING || state->units[i].type == TYPE_AGGRESSOR || state->units[i].type == TYPE_REPELLER) {
                float force_x = 0;
                float force_y = 0;
                
                int gx = (int)(ux / CELL_SIZE);
                int gy = (int)(uy / CELL_SIZE);
                
                float align_x = 0, align_y = 0;
                float coh_x = 0, coh_y = 0;
                int boid_count = 0;
                
                float closest_enemy_sq = 200.0f * 200.0f; // Limit aggro range
                float closest_enemy_dx = 0;
                float closest_enemy_dy = 0;
                float closest_enemy_health = 0;
                bool found_enemy = false;

                for (int cx = gx - 1; cx <= gx + 1; cx++) {
                    if (cx < 0 || cx >= GRID_WIDTH) continue;
                    for (int cy = gy - 1; cy <= gy + 1; cy++) {
                        if (cy < 0 || cy >= GRID_HEIGHT) continue;
                        int j = state->grid.head[cx][cy];
                        while (j != -1) {
                            if (i != j && state->units[j].active) {
                                float dx = ux - state->units[j].position.x;
                                float dy = uy - state->units[j].position.y;
                                float dist_sq = dx*dx + dy*dy;
                                
                                if (state->units[j].team == state->units[i].team) {
                                    if (state->units[j].type == state->units[i].type && dist_sq < 40.0f * 40.0f) {
                                        align_x += state->units[j].velocity.x;
                                        align_y += state->units[j].velocity.y;
                                        coh_x += state->units[j].position.x;
                                        coh_y += state->units[j].position.y;
                                        boid_count++;
                                    }
                                } else {
                                    if (dist_sq < closest_enemy_sq) {
                                        closest_enemy_sq = dist_sq;
                                        closest_enemy_dx = dx;
                                        closest_enemy_dy = dy;
                                        closest_enemy_health = state->units[j].health;
                                        found_enemy = true;
                                    }
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
                
                // Aggressor/Repeller forces based on Strength (Health)
                if (found_enemy) {
                    float dist_e = sqrtf(closest_enemy_sq);
                    if (dist_e > 0.1f) {
                        bool local_advantage = (state->units[i].health >= closest_enemy_health);
                        
                        if (state->units[i].type == TYPE_AGGRESSOR) {
                            if (local_advantage) {
                                force_x -= (closest_enemy_dx / dist_e) * 0.5f; // Hard charge if stronger
                                force_y -= (closest_enemy_dy / dist_e) * 0.5f;
                            } else {
                                force_x -= (closest_enemy_dx / dist_e) * 0.1f; // Slight approach if weaker
                                force_y -= (closest_enemy_dy / dist_e) * 0.1f;
                            }
                        } else if (state->units[i].type == TYPE_REPELLER) {
                            if (dist_e < 120.0f) {
                                if (!local_advantage) {
                                    force_x += (closest_enemy_dx / dist_e) * 0.7f; // Kite swiftly if weaker
                                    force_y += (closest_enemy_dy / dist_e) * 0.7f;
                                } else {
                                    force_x -= (closest_enemy_dx / dist_e) * 0.2f; // Push forward if stronger
                                    force_y -= (closest_enemy_dy / dist_e) * 0.2f;
                                }
                            }
                        }
                    }
                }

                if (state->units[i].state == UNIT_MOVING) {
                    float tdx = state->units[i].target_pos.x - ux;
                    float tdy = state->units[i].target_pos.y - uy;
                    float tdist = sqrtf(tdx*tdx + tdy*tdy);
                    
                    if (tdist > state->units[i].radius * 1.5f) {
                         force_x += (tdx / tdist) * 0.2f;
                         force_y += (tdy / tdist) * 0.2f;
                    } else {
                         state->units[i].state = UNIT_IDLE;
                    }
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
            
        } else if (state->units[i].type < TYPE_HQ) {
            if (state->units[i].state == UNIT_MOVING) {
                float dx = state->units[i].target_pos.x - ux;
                float dy = state->units[i].target_pos.y - uy;
                float dist = sqrtf(dx * dx + dy * dy);
                
                if (dist > state->units[i].radius * 1.5f) {
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

__global__ void kernel_tick_combat(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS && state->units[i].active) {
        
        if (state->units[i].attack_cooldown > 0) {
            state->units[i].attack_cooldown--;
        }
        if (state->units[i].attack_fx_timer > 0) {
            state->units[i].attack_fx_timer--;
        }

        if (state->units[i].attack_cooldown == 0) {
            if (state->units[i].type == TYPE_FACTORY || state->units[i].type == TYPE_GENERATOR) return;

            float ux = state->units[i].position.x;
            float uy = state->units[i].position.y;
            int gx = (int)(ux / CELL_SIZE);
            int gy = (int)(uy / CELL_SIZE);
            
            float attack_range_sq = 60.0f * 60.0f;
            float damage = 15.0f;
            int cooldown = 45; // 0.75 seconds
            int fx = 6;
            int sr = 1;
            
            if (state->units[i].type == TYPE_HQ) {
                attack_range_sq = 200.0f * 200.0f;
                damage = 100.0f; // One shot
                cooldown = 30; // 0.5 seconds
                fx = 15;
                sr = 4; // Check 4 cells out
            }

            float closest_sq = attack_range_sq;
            int target_idx = -1;

            for (int cx = gx - sr; cx <= gx + sr; cx++) {
                if (cx < 0 || cx >= GRID_WIDTH) continue;
                for (int cy = gy - sr; cy <= gy + sr; cy++) {
                    if (cy < 0 || cy >= GRID_HEIGHT) continue;

                    int j = state->grid.head[cx][cy];
                    while (j != -1) {
                        if (state->units[j].active && state->units[j].team != state->units[i].team && state->units[j].health > 0) {
                            float dx = state->units[j].position.x - ux;
                            float dy = state->units[j].position.y - uy;
                            float dsq = dx*dx + dy*dy;
                            if (dsq < closest_sq) {
                                closest_sq = dsq;
                                target_idx = j;
                            }
                        }
                        j = state->grid.next[j];
                    }
                }
            }

            if (target_idx != -1) { // Apply damage safely across threads
                atomicAdd(&state->units[target_idx].health, -damage);
                state->units[i].attack_cooldown = cooldown;
                state->units[i].attack_fx_timer = fx;
                state->units[i].last_attack_target = state->units[target_idx].position;
            }
        }
    }
}

__global__ void kernel_tick_combat_death(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < MAX_UNITS && state->units[i].active && state->units[i].health <= 0) {
        if (state->units[i].type == TYPE_HQ) {
            state->players[state->units[i].team].defeated = true;
        }
        state->units[i].active = false;
    }
}

__global__ void kernel_tick_economy(GameState* state) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Only one thread should handle passive income
    if (i == 0) {
        if (!state->players[TEAM_BLUE].defeated && !state->players[TEAM_RED].defeated) {
            atomicAdd(&state->players[TEAM_BLUE].points, 1.0f / 60.0f);
            atomicAdd(&state->players[TEAM_RED].points, 1.0f / 60.0f);
        }
    }
    
    if (i < MAX_UNITS && state->units[i].active) {
        if (state->players[TEAM_BLUE].defeated || state->players[TEAM_RED].defeated) return;
        
        if (state->units[i].type == TYPE_HQ) {
            atomicAdd(&state->players[state->units[i].team].points, 2.0f / 60.0f);
        } else if (state->units[i].type == TYPE_GENERATOR) {
            atomicAdd(&state->players[state->units[i].team].points, 5.0f / 60.0f);
        } else if (state->units[i].type == TYPE_FACTORY) {
            if ((state->tick_counter + i) % 180 == 0) { // Produce 1 free unit every 3 seconds
                float sx = state->units[i].position.x + 30.0f * (state->units[i].team == TEAM_BLUE ? 1 : -1);
                float sy = state->units[i].position.y + (((i % 3) - 1) * 15.0f);
                
                int r = (state->tick_counter + i) % 100;
                UnitType type = TYPE_BASE;
                if (r < 10) type = TYPE_BRUISER;
                else if (r < 20) type = TYPE_AGGRESSOR;
                else if (r < 25) type = TYPE_REPELLER;
                else if (r < 30) type = TYPE_MEDIC;
                
                game_spawn_unit(state, state->units[i].team, type, sx, sy);
            }
        }
    }
}

void game_tick(GameState* state) {
    state->tick_counter++;
    
    // AI decisions (CPU side)
    ai_tick(state, TEAM_RED);
    
    int threads = 256;
    int blocks = (MAX_UNITS + threads - 1) / threads;

    kernel_tick_economy<<<blocks, threads>>>(state);
    kernel_tick_medics<<<blocks, threads>>>(state);
    kernel_tick_bruisers<<<blocks, threads>>>(state);
    
    kernel_tick_movement<<<blocks, threads>>>(state);
    
    kernel_resolve_collisions<<<blocks, threads>>>(state);
    
    kernel_tick_combat<<<blocks, threads>>>(state);
    kernel_tick_combat_death<<<blocks, threads>>>(state);
    
    kernel_grid_clear<<<(GRID_WIDTH * GRID_HEIGHT + threads - 1)/threads, threads>>>(state);
    kernel_grid_build<<<blocks, threads>>>(state);
    
    cudaDeviceSynchronize();
}

void command_move_selected(GameState* state, float target_x, float target_y) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!state->units[i].active || !state->units[i].selected || state->units[i].type >= TYPE_HQ) continue;
        
        state->units[i].state = UNIT_MOVING;
        state->units[i].target_pos.x = target_x;
        state->units[i].target_pos.y = target_y;
    }
}

*/