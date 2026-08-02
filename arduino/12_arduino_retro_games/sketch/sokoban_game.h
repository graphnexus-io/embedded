#ifndef RETRO_GAMES_SOKOBAN_GAME_H
#define RETRO_GAMES_SOKOBAN_GAME_H

#include <Arduino.h>

#include "serial_input.h"

enum class SokobanUpdateResult : uint8_t {
    RUNNING,
    WON,
};

void sokoban_level_select_enter();
bool sokoban_level_select_update(InputEvent event);
void sokoban_game_start_selected();
void sokoban_game_start_next();
SokobanUpdateResult sokoban_game_update(InputEvent event);
void sokoban_game_show_paused();
void sokoban_game_resume();
void sokoban_game_show_won();

#endif
