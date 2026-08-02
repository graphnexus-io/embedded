#ifndef RETRO_GAMES_GAME_MENU_H
#define RETRO_GAMES_GAME_MENU_H

#include <Arduino.h>

#include "serial_input.h"

enum class GameId : uint8_t {
    NONE,
    SNAKE,
    TETRIS,
    BREAKOUT,
    RACING,
    GAME_2048,
    SOKOBAN,
    LUNAR_LANDER,
};

void game_menu_enter();
GameId game_menu_handle_input(InputEvent event);

#endif
