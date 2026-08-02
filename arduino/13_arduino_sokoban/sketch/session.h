#ifndef SOKOBAN_100_SESSION_H
#define SOKOBAN_100_SESSION_H

#include <Arduino.h>

#include "config.h"

enum class GameMode : uint8_t {
    FREE_SELECT = 1U,
    CAMPAIGN = 2U,
};

struct SessionState {
    bool active;
    GameMode mode;
    uint16_t level_index;
    uint16_t free_level;
    // Zero-based index of the next unsolved campaign level. LEVEL_COUNT means
    // that the complete campaign has been solved.
    uint16_t campaign_next;
    uint32_t campaign_score;
    uint32_t level_score;
    uint16_t moves;
    uint16_t pushes;
    uint8_t player_x;
    uint8_t player_y;
    uint8_t crate_count;
    uint8_t crate_x[MAX_CRATES];
    uint8_t crate_y[MAX_CRATES];
};

#endif
