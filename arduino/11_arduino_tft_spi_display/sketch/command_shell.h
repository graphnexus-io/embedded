#ifndef MINIOS_COMMAND_SHELL_H
#define MINIOS_COMMAND_SHELL_H

#include <Arduino.h>

#include "commands.h"

void shell_init();
void shell_poll();
uint8_t shell_history_count();
const char *shell_history_get(uint8_t chronological_index);
CommandStatus shell_last_status();

#endif
