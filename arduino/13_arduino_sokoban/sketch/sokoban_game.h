#ifndef SOKOBAN_100_GAME_H
#define SOKOBAN_100_GAME_H

#include <Arduino.h>

#include "serial_input.h"
#include "session.h"

enum class SokobanActionResult : uint8_t {
    NO_CHANGE,
    CHANGED,
    WON,
};

bool sokoban_game_start(
    uint16_t level_index, GameMode mode, SessionState *session);
bool sokoban_game_restore(SessionState *session);
SokobanActionResult sokoban_game_handle_input(
    InputEvent event, SessionState *session);
bool sokoban_game_is_solved();
void sokoban_game_redraw(const SessionState &session);
void sokoban_game_show_paused();
void sokoban_game_show_complete(const SessionState &session);

#endif
