#ifndef SOKOBAN_100_LEVEL_GENERATOR_H
#define SOKOBAN_100_LEVEL_GENERATOR_H

#include <Arduino.h>

#include "config.h"

enum Terrain : uint8_t {
    TERRAIN_FLOOR = 0U,
    TERRAIN_WALL,
    TERRAIN_TARGET,
};

struct LevelLayout {
    uint8_t terrain[LEVEL_CELL_COUNT];
    uint8_t player_x;
    uint8_t player_y;
    uint8_t crate_count;
    uint8_t crate_x[MAX_CRATES];
    uint8_t crate_y[MAX_CRATES];
    uint16_t scramble_pulls;
    uint16_t scramble_turns;
    uint8_t internal_walls;
    uint8_t moved_crates;
    // Manhattan assignment lower bound: every solution needs at least this
    // many crate pushes even if walls and player positioning are ignored.
    uint8_t minimum_push_distance;
    uint8_t generation_retry;
    uint8_t generation_attempts;
    uint32_t starting_score;
};

bool level_generate(uint16_t level_index, LevelLayout *layout);
bool level_entities_are_valid(const LevelLayout &layout);
uint8_t level_cell_index(uint8_t x, uint8_t y);

#endif
