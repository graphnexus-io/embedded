#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void systick_init(void);
void systick_wait_ms(uint32_t milliseconds);

#endif
