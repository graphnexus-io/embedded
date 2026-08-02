#ifndef RETRO_GAMES_TETRIS_GAME_H
#define RETRO_GAMES_TETRIS_GAME_H

#include <Arduino.h>

#include "serial_input.h"

enum class TetrisUpdateResult : uint8_t {
    RUNNING,
    GAME_OVER,
};

void tetris_game_start(uint32_t now_ms);
TetrisUpdateResult tetris_game_update(InputEvent event, uint32_t now_ms);
void tetris_game_show_paused();
void tetris_game_resume(uint32_t now_ms);
void tetris_game_show_game_over();

#endif
