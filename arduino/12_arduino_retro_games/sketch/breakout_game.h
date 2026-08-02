#ifndef RETRO_GAMES_BREAKOUT_GAME_H
#define RETRO_GAMES_BREAKOUT_GAME_H

#include <Arduino.h>

#include "serial_input.h"

enum class BreakoutUpdateResult : uint8_t {
    RUNNING,
    LOST,
    WON,
};

void breakout_game_start(uint32_t now_ms);
BreakoutUpdateResult breakout_game_update(InputEvent event, uint32_t now_ms);
void breakout_game_show_paused();
void breakout_game_resume(uint32_t now_ms);
void breakout_game_show_result(bool won);

#endif
