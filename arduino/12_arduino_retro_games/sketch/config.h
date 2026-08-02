#ifndef RETRO_GAMES_CONFIG_H
#define RETRO_GAMES_CONFIG_H

#include <Arduino.h>

#define APP_NAME "Retro Games"
#define APP_VERSION "0.5.0"

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
// Flip either flag if the physical module is mounted in the opposite
// orientation. Defaults map low X to left and low Y to up.
constexpr bool JOYSTICK_REVERSE_X = false;
constexpr bool JOYSTICK_REVERSE_Y = false;
constexpr uint16_t JOYSTICK_ACTIVATE_DELTA = 220U;
constexpr uint16_t JOYSTICK_RELEASE_DELTA = 130U;
constexpr uint16_t JOYSTICK_SAMPLE_INTERVAL_MS = 15U;
constexpr uint16_t JOYSTICK_INITIAL_REPEAT_MS = 330U;
constexpr uint16_t JOYSTICK_REPEAT_MS = 150U;
constexpr uint16_t JOYSTICK_BUTTON_DEBOUNCE_MS = 25U;
constexpr uint16_t JOYSTICK_PAUSE_HOLD_MS = 700U;
constexpr uint16_t JOYSTICK_BACK_HOLD_MS = 1500U;

constexpr int16_t SCREEN_WIDTH = 480;
constexpr int16_t SCREEN_HEIGHT = 320;

constexpr int16_t LCD_PANEL_X = 24;
constexpr int16_t LCD_PANEL_Y = 12;
constexpr int16_t LCD_PANEL_WIDTH = 432;
constexpr int16_t LCD_PANEL_HEIGHT = 296;

constexpr uint8_t SNAKE_COLS = 28U;
constexpr uint8_t SNAKE_ROWS = 16U;
constexpr uint8_t CELL_SIZE = 14U;
constexpr uint16_t MAX_SNAKE_LENGTH =
    static_cast<uint16_t>(SNAKE_COLS) * static_cast<uint16_t>(SNAKE_ROWS);
constexpr uint8_t INITIAL_SNAKE_LENGTH = 4U;

constexpr int16_t PLAYFIELD_X = 44;
constexpr int16_t PLAYFIELD_Y = 68;
constexpr int16_t PLAYFIELD_WIDTH =
    static_cast<int16_t>(SNAKE_COLS) * CELL_SIZE;
constexpr int16_t PLAYFIELD_HEIGHT =
    static_cast<int16_t>(SNAKE_ROWS) * CELL_SIZE;

constexpr uint16_t INITIAL_MOVE_INTERVAL_MS = 250U;
constexpr uint16_t MOVE_INTERVAL_STEP_MS = 10U;
constexpr uint16_t MINIMUM_MOVE_INTERVAL_MS = 80U;
constexpr uint8_t FOOD_RANDOM_ATTEMPTS = 64U;

constexpr uint8_t TETRIS_COLS = 10U;
constexpr uint8_t TETRIS_ROWS = 20U;
constexpr uint8_t TETRIS_CELL_SIZE = 12U;
constexpr int16_t TETRIS_BOARD_X = 82;
constexpr int16_t TETRIS_BOARD_Y = 56;
constexpr int16_t TETRIS_BOARD_WIDTH =
    static_cast<int16_t>(TETRIS_COLS) * TETRIS_CELL_SIZE;
constexpr int16_t TETRIS_BOARD_HEIGHT =
    static_cast<int16_t>(TETRIS_ROWS) * TETRIS_CELL_SIZE;
constexpr uint16_t TETRIS_INITIAL_FALL_MS = 600U;
constexpr uint16_t TETRIS_LEVEL_STEP_MS = 50U;
constexpr uint16_t TETRIS_MINIMUM_FALL_MS = 100U;
constexpr uint8_t TETRIS_LINES_PER_LEVEL = 10U;

constexpr int16_t BREAKOUT_FIELD_X = 52;
constexpr int16_t BREAKOUT_FIELD_Y = 50;
constexpr int16_t BREAKOUT_FIELD_WIDTH = 376;
constexpr int16_t BREAKOUT_FIELD_HEIGHT = 244;
constexpr uint8_t BREAKOUT_BRICK_COLS = 10U;
constexpr uint8_t BREAKOUT_BRICK_ROWS = 5U;
constexpr int16_t BREAKOUT_BRICK_START_X = 60;
constexpr int16_t BREAKOUT_BRICK_START_Y = 66;
constexpr uint8_t BREAKOUT_BRICK_PITCH_X = 36U;
constexpr uint8_t BREAKOUT_BRICK_PITCH_Y = 20U;
constexpr uint8_t BREAKOUT_BRICK_WIDTH = 32U;
constexpr uint8_t BREAKOUT_BRICK_HEIGHT = 14U;
constexpr uint8_t BREAKOUT_PADDLE_WIDTH = 64U;
constexpr uint8_t BREAKOUT_PADDLE_HEIGHT = 8U;
constexpr uint8_t BREAKOUT_PADDLE_STEP = 22U;
constexpr int16_t BREAKOUT_PADDLE_Y = 276;
constexpr uint8_t BREAKOUT_BALL_SIZE = 7U;
constexpr uint16_t BREAKOUT_BALL_INTERVAL_MS = 28U;
constexpr uint8_t BREAKOUT_INITIAL_LIVES = 3U;

constexpr uint8_t SOKOBAN_MAX_COLS = 22U;
constexpr uint8_t SOKOBAN_MAX_ROWS = 13U;
constexpr uint8_t SOKOBAN_TILE_SIZE = 18U;
constexpr int16_t SOKOBAN_PLAY_LEFT = 36;
constexpr int16_t SOKOBAN_PLAY_TOP = 62;
constexpr int16_t SOKOBAN_PLAY_WIDTH = 408;
constexpr int16_t SOKOBAN_PLAY_HEIGHT = 238;

// Restrained monochrome-LCD palette in RGB565.
constexpr uint16_t COLOR_OUTER = 0x1082U;
constexpr uint16_t COLOR_LCD = 0xCEB0U;
constexpr uint16_t COLOR_LCD_SHADE = 0x9D0CU;
constexpr uint16_t COLOR_INK = 0x2143U;
constexpr uint16_t COLOR_INK_LIGHT = 0x52A7U;

#endif
