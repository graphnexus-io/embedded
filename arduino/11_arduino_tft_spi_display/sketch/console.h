#ifndef MINIOS_CONSOLE_H
#define MINIOS_CONSOLE_H

#include <Arduino.h>

void console_print(const char *text);
void console_print(const __FlashStringHelper *text);
void console_println();
void console_println(const char *text);
void console_println(const __FlashStringHelper *text);
void console_write(char character);
void console_print_u32(uint32_t value);
void console_println_u32(uint32_t value);
void console_print_i32(int32_t value);
void console_println_i32(int32_t value);

#endif
