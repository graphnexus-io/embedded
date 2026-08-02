#ifndef RETRO_GAMES_GAME_2048_H
#define RETRO_GAMES_GAME_2048_H

#include <Arduino.h>

#include "serial_input.h"

enum class Game2048UpdateResult : uint8_t {
    RUNNING,
    LOST,
    WON,
};

void game_2048_start();
Game2048UpdateResult game_2048_update(InputEvent event);
void game_2048_show_paused();
void game_2048_resume();
void game_2048_show_result(bool won);

#endif
