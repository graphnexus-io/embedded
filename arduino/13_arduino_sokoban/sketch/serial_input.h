#ifndef SOKOBAN_100_SERIAL_INPUT_H
#define SOKOBAN_100_SERIAL_INPUT_H

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
    UNDO,
    PAGE_PREVIOUS,
    PAGE_NEXT,
};

void serial_input_init();
InputEvent serial_input_poll();

#endif
