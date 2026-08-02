#ifndef SOKOBAN_100_JOYSTICK_INPUT_H
#define SOKOBAN_100_JOYSTICK_INPUT_H

#include <Arduino.h>

#include "serial_input.h"

void joystick_input_init();
InputEvent joystick_input_poll(uint32_t now_ms);

#endif
