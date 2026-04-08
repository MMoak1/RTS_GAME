#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_UNITS 20000
#define MAX_BUILDINGS 1000

typedef enum {
    TEAM_BLUE = 0,
    TEAM_RED = 1
} Team;

typedef enum {
    UNIT_IDLE = 0,
    UNIT_MOVING,
    UNIT_ATTACKING,
    UNIT_GATHERING
} UnitState;

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    bool active;
    Team team;
    Vec2 position;
    Vec2 target_pos;
    UnitState state;
    float health;
    float max_health;
} Unit;

typedef struct {
    bool active;
    Team team;
    Vec2 position;
    float health;
} Building;

typedef struct {
    Unit units[MAX_UNITS];
    int unit_count; // Number of currently active units, or max active index
    
    Building buildings[MAX_BUILDINGS];
    int building_count;
    
    // Global state
    uint32_t tick_counter;
} GameState;

// Initializes the game state
void game_init(GameState* state);

// Runs one fixed step of logic
void game_tick(GameState* state);

#endif // GAME_H
