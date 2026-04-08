#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_UNITS 20000
#define MAX_BUILDINGS 1000

#define GRID_WIDTH 40
#define GRID_HEIGHT 40
#define CELL_SIZE 50

typedef enum {
    TEAM_BLUE = 0,
    TEAM_RED = 1
} Team;

typedef enum {
    TYPE_BASE = 0,
    TYPE_MEDIC,
    TYPE_BRUISER,
    TYPE_AGGRESSOR,
    TYPE_REPELLER
} UnitType;

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
    bool selected;
    Team team;
    UnitType type;
    Vec2 position;
    Vec2 target_pos;
    Vec2 velocity;
    UnitState state;
    float health;
    float max_health;
    uint8_t attack_cooldown;
    uint8_t attack_fx_timer;
    Vec2 last_attack_target;
} Unit;

typedef struct {
    bool active;
    Team team;
    Vec2 position;
    float health;
} Building;

typedef struct {
    int head[GRID_WIDTH][GRID_HEIGHT]; // Index of first unit in cell. -1 if empty.
    int next[MAX_UNITS]; // Index of next unit in same cell. -1 if end.
} SpatialGrid;

typedef struct {
    Unit units[MAX_UNITS];
    int unit_count; // Number of currently active units, or max active index
    
    Building buildings[MAX_BUILDINGS];
    int building_count;
    
    SpatialGrid grid;
    
    // Global state
    uint32_t tick_counter;
} GameState;

// Initializes the game state
void game_init(GameState* state);

// Runs one fixed step of logic
void game_tick(GameState* state);

// Issues a move command
void command_move_selected(GameState* state, float target_x, float target_y);

#endif // GAME_H
