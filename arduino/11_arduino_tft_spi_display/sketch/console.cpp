#include "console.h"

#include <avr/pgmspace.h>

#include "tft_terminal.h"

void console_print(const char *text)
{
    if (text == nullptr) {
        return;
    }
    Serial.print(text);
    terminal_print(text);
}

void console_print(const __FlashStringHelper *text)
{
    if (text == nullptr) {
        return;
    }

    Serial.print(text);
    const char *address = reinterpret_cast<const char *>(text);
    char character = static_cast<char>(pgm_read_byte(address));
    while (character != '\0') {
        terminal_write(character);
        ++address;
        character = static_cast<char>(pgm_read_byte(address));
    }
    terminal_refresh();
}

void console_println()
{
    Serial.println();
    terminal_println("");
}

void console_println(const char *text)
{
    if (text == nullptr) {
        console_println();
        return;
    }
    Serial.println(text);
    terminal_println(text);
}

void console_println(const __FlashStringHelper *text)
{
    console_print(text);
    console_println();
}

void console_write(char character)
{
    Serial.write(static_cast<uint8_t>(character));
    terminal_write(character);
    terminal_refresh();
}

void console_print_u32(uint32_t value)
{
    char number[11];
    ultoa(value, number, 10);
    console_print(number);
}

void console_println_u32(uint32_t value)
{
    console_print_u32(value);
    console_println();
}

void console_print_i32(int32_t value)
{
    char number[12];
    ltoa(value, number, 10);
    console_print(number);
}

void console_println_i32(int32_t value)
{
    console_print_i32(value);
    console_println();
}
