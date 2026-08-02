#ifndef RETRO_GAMES_DISPLAY_HAL_H
#define RETRO_GAMES_DISPLAY_HAL_H

#include <Arduino.h>

void display_hardware_init();
void display_init();
void display_clear(uint16_t color);
void display_fill_rect(
    int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
void display_draw_rect(
    int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
void display_draw_line(
    int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void display_text(
    int16_t x,
    int16_t y,
    const char *text,
    uint8_t scale,
    uint16_t foreground,
    uint16_t background);
void display_draw_lcd_panel();

#endif
