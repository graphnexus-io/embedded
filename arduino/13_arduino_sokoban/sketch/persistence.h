#ifndef SOKOBAN_100_PERSISTENCE_H
#define SOKOBAN_100_PERSISTENCE_H

#include <Arduino.h>

#include "session.h"

void persistence_set_defaults(SessionState *state);
bool persistence_load(SessionState *state);
bool persistence_save(const SessionState &state);

#endif
