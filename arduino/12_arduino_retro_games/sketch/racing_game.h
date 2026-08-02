#ifndef RETRO_GAMES_RACING_GAME_H
#define RETRO_GAMES_RACING_GAME_H

#include <Arduino.h>

#include "serial_input.h"

enum class RacingUpdateResult : uint8_t {
    RUNNING,
    GAME_OVER,
};

void racing_game_start(uint32_t now_ms);
RacingUpdateResult racing_game_update(InputEvent event, uint32_t now_ms);
void racing_game_show_paused();
void racing_game_resume(uint32_t now_ms);
void racing_game_show_game_over();

#endif
