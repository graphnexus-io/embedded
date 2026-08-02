#include "display_hal.h"

#include <LCDWIKI_GUI.h>
#include <LCDWIKI_SPI.h>
#include <SPI.h>

#include "config.h"

namespace {

LCDWIKI_SPI display(ST7796S, LCD_CS_PIN, LCD_RS_PIN, LCD_RST_PIN, LCD_LED_PIN);

void prepare_display_bus()
{
    // Same shared-bus setup used by 11_arduino_tft_spi_display.
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
}

}  // namespace

void display_hardware_init()
{
    // Match project 11: deselect both peripherals and force hardware SS high
    // before Serial or normal display initialization.
    pinMode(LCD_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(HW_SS_PIN, OUTPUT);
    digitalWrite(LCD_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    digitalWrite(HW_SS_PIN, HIGH);
}

void display_init()
{
    prepare_display_bus();
    display.Init_LCD();
    display.Set_Rotation(1);
    display.Set_Text_Mode(0);
    display.Fill_Screen(COLOR_OUTER);
}

void display_clear(uint16_t color)
{
    prepare_display_bus();
    display.Fill_Screen(color);
}

void display_fill_rect(
    int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    prepare_display_bus();
    display.Set_Draw_color(color);
    display.Fill_Rectangle(
        x, y, static_cast<int16_t>(x + width - 1),
        static_cast<int16_t>(y + height - 1));
}

void display_draw_rect(
    int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    prepare_display_bus();
    display.Set_Draw_color(color);
    display.Draw_Rectangle(
        x, y, static_cast<int16_t>(x + width - 1),
        static_cast<int16_t>(y + height - 1));
}

void display_draw_line(
    int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    prepare_display_bus();
    display.Set_Draw_color(color);
    display.Draw_Line(x1, y1, x2, y2);
}

void display_text(
    int16_t x,
    int16_t y,
    const char *text,
    uint8_t scale,
    uint16_t foreground,
    uint16_t background)
{
    if (text == nullptr) {
        return;
    }
    prepare_display_bus();
    display.Set_Text_Mode(0);
    display.Set_Text_colour(foreground);
    display.Set_Text_Back_colour(background);
    display.Set_Text_Size(scale);
    display.Print_String(
        reinterpret_cast<uint8_t *>(const_cast<char *>(text)), x, y);
}

void display_draw_lcd_panel()
{
    display_clear(COLOR_OUTER);
    display_fill_rect(
        LCD_PANEL_X, LCD_PANEL_Y, LCD_PANEL_WIDTH, LCD_PANEL_HEIGHT, COLOR_LCD);
    display_draw_rect(
        LCD_PANEL_X, LCD_PANEL_Y, LCD_PANEL_WIDTH, LCD_PANEL_HEIGHT, COLOR_INK);
    display_draw_rect(
        LCD_PANEL_X + 3, LCD_PANEL_Y + 3,
        LCD_PANEL_WIDTH - 6, LCD_PANEL_HEIGHT - 6, COLOR_LCD_SHADE);
}
