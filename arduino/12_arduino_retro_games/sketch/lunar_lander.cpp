#include "lunar_lander.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

constexpr int16_t FIXED_SCALE = 16;
constexpr int16_t FIELD_LEFT = 50;
constexpr int16_t FIELD_RIGHT = 430;
constexpr int16_t FIELD_TOP = 56;
constexpr int16_t FIELD_BOTTOM = 292;
constexpr int16_t PAD_LEFT = 204;
constexpr int16_t PAD_RIGHT = 276;
constexpr int16_t PAD_Y = 278;
constexpr int16_t LANDER_HALF_WIDTH = 12;
constexpr int16_t LANDER_HEIGHT = 22;
constexpr int16_t GRAVITY = 1;
constexpr int16_t MAIN_THRUST = 6;
constexpr int16_t SIDE_THRUST = 2;
constexpr int16_t MAX_HORIZONTAL_SPEED = 24;
constexpr int16_t MAX_DOWN_SPEED = 80;
constexpr int16_t MAX_UP_SPEED = -40;
constexpr int16_t SAFE_VERTICAL_SPEED = 10;
constexpr int16_t SAFE_HORIZONTAL_SPEED = 6;
constexpr uint16_t INITIAL_FUEL = 150U;
constexpr uint16_t PHYSICS_INTERVAL_MS = 50U;
constexpr uint16_t STATS_INTERVAL_MS = 200U;

int32_t lander_x_fixed = 0L;
int32_t lander_y_fixed = 0L;
int16_t horizontal_speed = 0;
int16_t vertical_speed = 0;
uint16_t fuel = INITIAL_FUEL;
uint32_t last_physics_ms = 0UL;
uint32_t last_stats_ms = 0UL;
bool main_thrust_flash = false;

int16_t lander_x()
{
    return static_cast<int16_t>(lander_x_fixed / FIXED_SCALE);
}

int16_t lander_y()
{
    return static_cast<int16_t>(lander_y_fixed / FIXED_SCALE);
}

int16_t terrain_height(int16_t x)
{
    if (x < 110) {
        return 250;
    }
    if (x < 170) {
        return 266;
    }
    if (x < PAD_LEFT) {
        return 282;
    }
    if (x < PAD_RIGHT) {
        return PAD_Y;
    }
    if (x < 320) {
        return 268;
    }
    if (x < 370) {
        return 254;
    }
    return 272;
}

int16_t ground_below_lander()
{
    int16_t ground = terrain_height(lander_x());
    const int16_t left_ground = terrain_height(
        static_cast<int16_t>(lander_x() - LANDER_HALF_WIDTH));
    const int16_t right_ground = terrain_height(
        static_cast<int16_t>(lander_x() + LANDER_HALF_WIDTH));
    if (left_ground < ground) {
        ground = left_ground;
    }
    if (right_ground < ground) {
        ground = right_ground;
    }
    return ground;
}

void draw_terrain_segment(int16_t left, int16_t right, int16_t top)
{
    display_fill_rect(left, top, right - left, FIELD_BOTTOM - top,
                      COLOR_INK_LIGHT);
    display_fill_rect(left, top, right - left, 3, COLOR_INK);
}

void draw_terrain()
{
    draw_terrain_segment(FIELD_LEFT, 110, 250);
    draw_terrain_segment(110, 170, 266);
    draw_terrain_segment(170, PAD_LEFT, 282);
    draw_terrain_segment(PAD_LEFT, PAD_RIGHT, PAD_Y);
    draw_terrain_segment(PAD_RIGHT, 320, 268);
    draw_terrain_segment(320, 370, 254);
    draw_terrain_segment(370, FIELD_RIGHT, 272);
    display_fill_rect(PAD_LEFT + 5, PAD_Y - 3,
                      PAD_RIGHT - PAD_LEFT - 10, 3, COLOR_INK);
    display_text(PAD_LEFT + 24, PAD_Y + 5, "PAD", 1U,
                 COLOR_LCD, COLOR_INK_LIGHT);
}

void draw_lander(bool thrust)
{
    const int16_t x = lander_x();
    const int16_t y = lander_y();
    display_fill_rect(x - 7, y, 14, 12, COLOR_INK);
    display_fill_rect(x - 4, y + 3, 8, 5, COLOR_LCD_SHADE);
    display_fill_rect(x - 11, y + 10, 4, 10, COLOR_INK);
    display_fill_rect(x + 7, y + 10, 4, 10, COLOR_INK);
    display_fill_rect(x - 12, y + 19, 8, 3, COLOR_INK);
    display_fill_rect(x + 4, y + 19, 8, 3, COLOR_INK);
    if (thrust) {
        display_fill_rect(x - 2, y + 13, 4, 7, COLOR_LCD);
        display_fill_rect(x - 1, y + 15, 2, 5, COLOR_INK_LIGHT);
    }
}

void erase_lander()
{
    display_fill_rect(
        lander_x() - LANDER_HALF_WIDTH, lander_y(),
        LANDER_HALF_WIDTH * 2, LANDER_HEIGHT, COLOR_LCD);
}

void append_text(char *destination, uint8_t capacity, const char *source)
{
    uint8_t position = static_cast<uint8_t>(strlen(destination));
    while (*source != '\0' && position + 1U < capacity) {
        destination[position++] = *source++;
    }
    destination[position] = '\0';
}

void append_number(char *destination, uint8_t capacity, int16_t value)
{
    char number[8];
    itoa(value, number, 10);
    append_text(destination, capacity, number);
}

void draw_statistics()
{
    char text[44] = "F ";
    char number[8];
    utoa(fuel, number, 10);
    append_text(text, sizeof(text), number);
    append_text(text, sizeof(text), "  V ");
    append_number(text, sizeof(text), vertical_speed);
    append_text(text, sizeof(text), "  H ");
    append_number(text, sizeof(text), horizontal_speed);
    const int16_t altitude = ground_below_lander() -
        (lander_y() + LANDER_HEIGHT);
    append_text(text, sizeof(text), "  A ");
    append_number(text, sizeof(text), altitude > 0 ? altitude : 0);

    display_fill_rect(170, 28, 264, 20, COLOR_LCD);
    display_text(176, 32, text, 1U, COLOR_INK, COLOR_LCD);
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(48, 30, "LANDER", 2U, COLOR_INK, COLOR_LCD);
    display_fill_rect(
        FIELD_LEFT, FIELD_TOP,
        FIELD_RIGHT - FIELD_LEFT, FIELD_BOTTOM - FIELD_TOP, COLOR_LCD);
    display_draw_rect(
        FIELD_LEFT - 2, FIELD_TOP - 2,
        FIELD_RIGHT - FIELD_LEFT + 4, FIELD_BOTTOM - FIELD_TOP + 4,
        COLOR_INK_LIGHT);
    draw_terrain();
    draw_lander(main_thrust_flash);
    draw_statistics();
}

void limit_speeds()
{
    if (horizontal_speed < -MAX_HORIZONTAL_SPEED) {
        horizontal_speed = -MAX_HORIZONTAL_SPEED;
    } else if (horizontal_speed > MAX_HORIZONTAL_SPEED) {
        horizontal_speed = MAX_HORIZONTAL_SPEED;
    }
    if (vertical_speed < MAX_UP_SPEED) {
        vertical_speed = MAX_UP_SPEED;
    } else if (vertical_speed > MAX_DOWN_SPEED) {
        vertical_speed = MAX_DOWN_SPEED;
    }
}

bool apply_thrust(InputEvent event)
{
    main_thrust_flash = false;
    if (event == InputEvent::UP && fuel >= 3U) {
        fuel = static_cast<uint16_t>(fuel - 3U);
        vertical_speed = static_cast<int16_t>(vertical_speed - MAIN_THRUST);
        main_thrust_flash = true;
    } else if (event == InputEvent::LEFT && fuel > 0U) {
        --fuel;
        horizontal_speed = static_cast<int16_t>(horizontal_speed - SIDE_THRUST);
    } else if (event == InputEvent::RIGHT && fuel > 0U) {
        --fuel;
        horizontal_speed = static_cast<int16_t>(horizontal_speed + SIDE_THRUST);
    } else {
        return false;
    }
    limit_speeds();
    return true;
}

void draw_overlay(const char *title, const char *line_two, const char *line_three)
{
    display_fill_rect(104, 114, 272, 98, COLOR_LCD_SHADE);
    display_draw_rect(104, 114, 272, 98, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 128, title, 2U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(156, 166, line_two, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(174, 188, line_three, 1U, COLOR_INK, COLOR_LCD_SHADE);
}

}  // namespace

void lunar_lander_start(uint32_t now_ms)
{
    lander_x_fixed = 240L * FIXED_SCALE;
    lander_y_fixed = 70L * FIXED_SCALE;
    horizontal_speed = static_cast<int16_t>(random(-4L, 5L));
    vertical_speed = 0;
    fuel = INITIAL_FUEL;
    last_physics_ms = now_ms;
    last_stats_ms = now_ms;
    main_thrust_flash = false;
    draw_complete_game();
}

LanderUpdateResult lunar_lander_update(InputEvent event, uint32_t now_ms)
{
    const bool stats_dirty = apply_thrust(event);
    if (static_cast<uint32_t>(now_ms - last_physics_ms) < PHYSICS_INTERVAL_MS) {
        if (stats_dirty) {
            erase_lander();
            draw_lander(main_thrust_flash);
            draw_statistics();
        }
        return LanderUpdateResult::RUNNING;
    }
    last_physics_ms = now_ms;

    erase_lander();
    vertical_speed = static_cast<int16_t>(vertical_speed + GRAVITY);
    limit_speeds();
    lander_x_fixed += horizontal_speed;
    lander_y_fixed += vertical_speed;

    const int16_t minimum_x = FIELD_LEFT + LANDER_HALF_WIDTH;
    const int16_t maximum_x = FIELD_RIGHT - LANDER_HALF_WIDTH;
    if (lander_x() < minimum_x) {
        lander_x_fixed = static_cast<int32_t>(minimum_x) * FIXED_SCALE;
        horizontal_speed = static_cast<int16_t>(abs(horizontal_speed));
    } else if (lander_x() > maximum_x) {
        lander_x_fixed = static_cast<int32_t>(maximum_x) * FIXED_SCALE;
        horizontal_speed = static_cast<int16_t>(-abs(horizontal_speed));
    }
    if (lander_y() < FIELD_TOP) {
        lander_y_fixed = static_cast<int32_t>(FIELD_TOP) * FIXED_SCALE;
        vertical_speed = 0;
    }

    const int16_t ground = ground_below_lander();
    if (lander_y() + LANDER_HEIGHT >= ground) {
        lander_y_fixed = static_cast<int32_t>(ground - LANDER_HEIGHT) * FIXED_SCALE;
        draw_lander(false);
        draw_statistics();
        const bool over_pad =
            lander_x() - LANDER_HALF_WIDTH >= PAD_LEFT &&
            lander_x() + LANDER_HALF_WIDTH < PAD_RIGHT;
        const bool safe_speed =
            vertical_speed >= 0 && vertical_speed <= SAFE_VERTICAL_SPEED &&
            abs(horizontal_speed) <= SAFE_HORIZONTAL_SPEED;
        return over_pad && safe_speed ?
            LanderUpdateResult::LANDED : LanderUpdateResult::CRASHED;
    }

    draw_lander(main_thrust_flash);
    main_thrust_flash = false;
    if (stats_dirty ||
        static_cast<uint32_t>(now_ms - last_stats_ms) >= STATS_INTERVAL_MS) {
        last_stats_ms = now_ms;
        draw_statistics();
    }
    return LanderUpdateResult::RUNNING;
}

void lunar_lander_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void lunar_lander_resume(uint32_t now_ms)
{
    last_physics_ms = now_ms;
    last_stats_ms = now_ms;
    main_thrust_flash = false;
    draw_complete_game();
}

void lunar_lander_show_result(bool landed)
{
    char status[30] = "V ";
    append_number(status, sizeof(status), vertical_speed);
    append_text(status, sizeof(status), "  H ");
    append_number(status, sizeof(status), horizontal_speed);
    append_text(status, sizeof(status), "  F ");
    char number[8];
    utoa(fuel, number, 10);
    append_text(status, sizeof(status), number);
    draw_overlay(landed ? "SOFT LANDING" : "CRASHED", status,
                 "PRESS RESTART  HOLD MENU");
}
