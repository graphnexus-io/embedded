#ifndef SOKOBAN_100_CONFIG_H
#define SOKOBAN_100_CONFIG_H

#include <Arduino.h>

#define APP_NAME "Sokoban 100"
#define APP_VERSION "2.0.0"

constexpr uint32_t SERIAL_BAUD_RATE = 115200UL;

constexpr uint8_t LCD_CS_PIN = 10U;
constexpr uint8_t LCD_RS_PIN = 9U;
constexpr uint8_t LCD_RST_PIN = 8U;
constexpr uint8_t LCD_LED_PIN = 5U;
constexpr uint8_t SD_CS_PIN = 4U;
constexpr uint8_t HW_SS_PIN = 53U;

constexpr uint8_t JOYSTICK_BUTTON_PIN = 2U;
constexpr uint8_t JOYSTICK_Y_PIN = A0;
constexpr uint8_t JOYSTICK_X_PIN = A1;
// Change either flag if a particular joystick module is mounted in the
// opposite orientation. With the defaults, low X is left and low Y is up.
constexpr bool JOYSTICK_REVERSE_X = false;
constexpr bool JOYSTICK_REVERSE_Y = false;
constexpr uint16_t JOYSTICK_ACTIVATE_DELTA = 220U;
constexpr uint16_t JOYSTICK_RELEASE_DELTA = 130U;
constexpr uint16_t JOYSTICK_SAMPLE_INTERVAL_MS = 15U;
constexpr uint16_t JOYSTICK_INITIAL_REPEAT_MS = 330U;
constexpr uint16_t JOYSTICK_REPEAT_MS = 150U;
constexpr uint16_t JOYSTICK_BUTTON_DEBOUNCE_MS = 25U;
constexpr uint16_t JOYSTICK_BUTTON_LONG_PRESS_MS = 900U;

constexpr int16_t SCREEN_WIDTH = 480;
constexpr int16_t SCREEN_HEIGHT = 320;
constexpr int16_t LCD_PANEL_X = 24;
constexpr int16_t LCD_PANEL_Y = 12;
constexpr int16_t LCD_PANEL_WIDTH = 432;
constexpr int16_t LCD_PANEL_HEIGHT = 296;

constexpr uint16_t LEVEL_COUNT = 100U;
constexpr uint8_t LEVEL_COLS = 16U;
constexpr uint8_t LEVEL_ROWS = 11U;
constexpr uint8_t LEVEL_CELL_COUNT = LEVEL_COLS * LEVEL_ROWS;
constexpr uint8_t MAX_CRATES = 6U;
constexpr uint8_t UNDO_DEPTH = 64U;

constexpr int16_t BOARD_CELL_SIZE = 20;
constexpr int16_t BOARD_X = 80;
constexpr int16_t BOARD_Y = 68;

constexpr uint16_t SAVE_IDLE_DELAY_MS = 900U;

// Menu and chrome palette in RGB565.
constexpr uint16_t COLOR_OUTER = 0x1082U;
constexpr uint16_t COLOR_LCD = 0xCEB0U;
constexpr uint16_t COLOR_LCD_SHADE = 0x9D0CU;
constexpr uint16_t COLOR_INK = 0x2143U;
constexpr uint16_t COLOR_INK_LIGHT = 0x52A7U;

// Classic PC Sokoban playfield palette. The stronger hue and luminance
// differences keep terrain and movable objects recognizable on inexpensive
// TFT panels without relying on subtle shading.
constexpr uint16_t COLOR_FLOOR = 0xD69AU;
constexpr uint16_t COLOR_FLOOR_DOT = 0xAD55U;
constexpr uint16_t COLOR_WALL_BRICK = 0xA0E3U;
constexpr uint16_t COLOR_WALL_LIGHT = 0xD986U;
constexpr uint16_t COLOR_WALL_DARK = 0x48E3U;
constexpr uint16_t COLOR_WALL_MORTAR = 0x738EU;
constexpr uint16_t COLOR_CRATE_WOOD = 0xE5C7U;
constexpr uint16_t COLOR_CRATE_LIGHT = 0xFF0DU;
constexpr uint16_t COLOR_CRATE_DARK = 0x5942U;
constexpr uint16_t COLOR_TARGET = 0xF269U;
constexpr uint16_t COLOR_TARGET_DARK = 0xB104U;
constexpr uint16_t COLOR_PLAYER_GREEN = 0x3666U;
constexpr uint16_t COLOR_PLAYER_GREEN_LIGHT = 0x6F4BU;
constexpr uint16_t COLOR_PLAYER_SKIN = 0xFE0EU;
constexpr uint16_t COLOR_PLAYER_BLUE = 0x2297U;
constexpr uint16_t COLOR_PLAYER_DARK = 0x10A2U;

#endif
