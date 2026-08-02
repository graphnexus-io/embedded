#ifndef RETRO_GAMES_SNAKE_GAME_H
#define RETRO_GAMES_SNAKE_GAME_H

#include <Arduino.h>

#include "serial_input.h"

enum class SnakeTickResult : uint8_t {
    RUNNING,
    LOST,
    WON,
};

void snake_game_start(uint32_t now_ms);
void snake_game_handle_direction(InputEvent event);
SnakeTickResult snake_game_update(uint32_t now_ms);
void snake_game_show_paused();
void snake_game_resume(uint32_t now_ms);
void snake_game_show_result(bool won);
uint16_t snake_game_score();

#endif
