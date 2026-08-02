#ifndef MINIOS_COMMANDS_H
#define MINIOS_COMMANDS_H

#include <Arduino.h>

enum CommandStatus : uint8_t {
    COMMAND_OK = 0,
    COMMAND_BAD_ARGUMENTS = 1,
    COMMAND_NOT_FOUND = 2,
    COMMAND_FAILED = 3,
    COMMAND_DENIED = 4,
};

CommandStatus commands_dispatch(int argc, char *argv[]);

#endif
