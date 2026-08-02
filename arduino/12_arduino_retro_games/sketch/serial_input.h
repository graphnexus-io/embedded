#ifndef RETRO_GAMES_SERIAL_INPUT_H
#define RETRO_GAMES_SERIAL_INPUT_H

#include <Arduino.h>

enum class InputEvent : uint8_t {
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    SELECT,
    PAUSE,
    RESTART,
    BACK,
};

void serial_input_init();
InputEvent serial_input_poll();

#endif
