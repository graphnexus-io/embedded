#ifndef RETRO_GAMES_LUNAR_LANDER_H
#define RETRO_GAMES_LUNAR_LANDER_H

#include <Arduino.h>

#include "serial_input.h"

enum class LanderUpdateResult : uint8_t {
    RUNNING,
    LANDED,
    CRASHED,
};

void lunar_lander_start(uint32_t now_ms);
LanderUpdateResult lunar_lander_update(InputEvent event, uint32_t now_ms);
void lunar_lander_show_paused();
void lunar_lander_resume(uint32_t now_ms);
void lunar_lander_show_result(bool landed);

#endif
