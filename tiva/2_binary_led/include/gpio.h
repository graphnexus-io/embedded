#ifndef GPIO_H
#define GPIO_H

#include <stdbool.h>
#include <stdint.h>

void gpio_init(void);
void gpio_binary_led_write(uint32_t value);
bool gpio_sw1_pressed_event(void);

#endif
