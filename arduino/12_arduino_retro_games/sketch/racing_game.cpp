#include "racing_game.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

constexpr int16_t ROAD_TOP = 54;
constexpr int16_t ROAD_BOTTOM = 292;
constexpr int16_t ROAD_X = 126;
constexpr int16_t ROAD_WIDTH = 228;
constexpr uint8_t LANE_COUNT = 3U;
constexpr int16_t LANE_WIDTH = ROAD_WIDTH / LANE_COUNT;
constexpr int16_t CAR_WIDTH = 34;
constexpr int16_t CAR_HEIGHT = 28;
constexpr int16_t PLAYER_Y = 252;
constexpr uint8_t OBSTACLE_COUNT = 4U;
constexpr int16_t SCROLL_STEP = 8;
constexpr int16_t DASH_HEIGHT = 18;
constexpr int16_t DASH_PERIOD = 42;
constexpr uint16_t INITIAL_TICK_MS = 110U;
constexpr uint16_t MINIMUM_TICK_MS = 55U;

struct ObstacleCar {
    int16_t y;
    uint8_t lane;
};

ObstacleCar obstacles[OBSTACLE_COUNT];
uint8_t player_lane = 1U;
uint8_t dash_offset = 0U;
uint32_t score = 0UL;
uint16_t tick_interval_ms = INITIAL_TICK_MS;
uint32_t last_tick_ms = 0UL;

int16_t lane_car_x(uint8_t lane)
{
    return static_cast<int16_t>(
        ROAD_X + static_cast<int16_t>(lane) * LANE_WIDTH +
        (LANE_WIDTH - CAR_WIDTH) / 2);
}

bool rectangles_overlap(
    int16_t first_x,
    int16_t first_y,
    int16_t first_width,
    int16_t first_height,
    int16_t second_x,
    int16_t second_y,
    int16_t second_width,
    int16_t second_height)
{
    return first_x < second_x + second_width &&
           first_x + first_width > second_x &&
           first_y < second_y + second_height &&
           first_y + first_height > second_y;
}

void fill_car_part(
    int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color)
{
    const int16_t clipped_y = y < ROAD_TOP ? ROAD_TOP : y;
    const int16_t clipped_bottom =
        y + height > ROAD_BOTTOM ? ROAD_BOTTOM : y + height;
    if (clipped_bottom > clipped_y) {
        display_fill_rect(x, clipped_y, width, clipped_bottom - clipped_y, color);
    }
}

void draw_car(int16_t x, int16_t y, bool player)
{
    fill_car_part(x + 5, y, CAR_WIDTH - 10, CAR_HEIGHT, COLOR_INK);
    fill_car_part(x, y + 7, CAR_WIDTH, CAR_HEIGHT - 14, COLOR_INK);
    fill_car_part(
        x + 8, y + 5, CAR_WIDTH - 16, CAR_HEIGHT - 10,
        player ? COLOR_LCD : COLOR_LCD_SHADE);
    fill_car_part(x + 3, y + 2, 5, 6, COLOR_INK_LIGHT);
    fill_car_part(x + CAR_WIDTH - 8, y + 2, 5, 6, COLOR_INK_LIGHT);
    fill_car_part(x + 3, y + CAR_HEIGHT - 8, 5, 6, COLOR_INK_LIGHT);
    fill_car_part(
        x + CAR_WIDTH - 8, y + CAR_HEIGHT - 8, 5, 6, COLOR_INK_LIGHT);
}

void erase_car(int16_t x, int16_t y)
{
    display_fill_rect(x, y, CAR_WIDTH, CAR_HEIGHT, COLOR_LCD_SHADE);
}

void draw_player()
{
    draw_car(lane_car_x(player_lane), PLAYER_Y, true);
}

void draw_obstacles()
{
    for (uint8_t index = 0U; index < OBSTACLE_COUNT; ++index) {
        if (obstacles[index].y + CAR_HEIGHT > ROAD_TOP &&
            obstacles[index].y < ROAD_BOTTOM) {
            draw_car(lane_car_x(obstacles[index].lane), obstacles[index].y, false);
        }
    }
}

void erase_obstacles()
{
    for (uint8_t index = 0U; index < OBSTACLE_COUNT; ++index) {
        const int16_t y = obstacles[index].y;
        if (y + CAR_HEIGHT <= ROAD_TOP || y >= ROAD_BOTTOM) {
            continue;
        }
        const int16_t clipped_y = y < ROAD_TOP ? ROAD_TOP : y;
        const int16_t clipped_bottom =
            y + CAR_HEIGHT > ROAD_BOTTOM ? ROAD_BOTTOM : y + CAR_HEIGHT;
        display_fill_rect(
            lane_car_x(obstacles[index].lane), clipped_y,
            CAR_WIDTH, clipped_bottom - clipped_y, COLOR_LCD_SHADE);
    }
}

void draw_dashes(uint8_t offset, uint16_t color)
{
    for (uint8_t divider = 1U; divider < LANE_COUNT; ++divider) {
        const int16_t x = static_cast<int16_t>(
            ROAD_X + static_cast<int16_t>(divider) * LANE_WIDTH - 2);
        for (int16_t y = static_cast<int16_t>(ROAD_TOP + offset - DASH_PERIOD);
             y < ROAD_BOTTOM;
             y = static_cast<int16_t>(y + DASH_PERIOD)) {
            const int16_t clipped_y = y < ROAD_TOP ? ROAD_TOP : y;
            const int16_t clipped_bottom =
                y + DASH_HEIGHT > ROAD_BOTTOM ? ROAD_BOTTOM : y + DASH_HEIGHT;
            if (clipped_bottom > clipped_y) {
                display_fill_rect(x, clipped_y, 4, clipped_bottom - clipped_y, color);
            }
        }
    }
}

void draw_score()
{
    char text[18] = "SCORE ";
    ultoa(score, &text[6], 10);
    display_fill_rect(284, 28, 148, 18, COLOR_LCD);
    display_text(290, 30, text, 2U, COLOR_INK, COLOR_LCD);
}

void draw_road()
{
    display_fill_rect(68, ROAD_TOP, 58, ROAD_BOTTOM - ROAD_TOP, COLOR_LCD);
    display_fill_rect(
        ROAD_X, ROAD_TOP, ROAD_WIDTH, ROAD_BOTTOM - ROAD_TOP, COLOR_LCD_SHADE);
    display_fill_rect(
        ROAD_X + ROAD_WIDTH, ROAD_TOP, 58, ROAD_BOTTOM - ROAD_TOP, COLOR_LCD);
    display_fill_rect(ROAD_X - 4, ROAD_TOP, 4, ROAD_BOTTOM - ROAD_TOP, COLOR_INK);
    display_fill_rect(
        ROAD_X + ROAD_WIDTH, ROAD_TOP, 4, ROAD_BOTTOM - ROAD_TOP, COLOR_INK);
    draw_dashes(dash_offset, COLOR_INK_LIGHT);
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(50, 30, "RETRO RACING", 2U, COLOR_INK, COLOR_LCD);
    draw_score();
    draw_road();
    draw_obstacles();
    draw_player();
}

void respawn_obstacle(uint8_t index)
{
    obstacles[index].y = static_cast<int16_t>(
        ROAD_TOP - CAR_HEIGHT - random(30L, 95L));
    uint8_t candidate = static_cast<uint8_t>(random(LANE_COUNT));
    for (uint8_t attempt = 0U; attempt < 6U; ++attempt) {
        bool lane_clear = true;
        for (uint8_t other = 0U; other < OBSTACLE_COUNT; ++other) {
            if (other != index && obstacles[other].lane == candidate &&
                abs(obstacles[other].y - obstacles[index].y) < 70) {
                lane_clear = false;
                break;
            }
        }
        if (lane_clear) {
            break;
        }
        candidate = static_cast<uint8_t>(random(LANE_COUNT));
    }
    obstacles[index].lane = candidate;
}

bool player_hit_obstacle()
{
    const int16_t player_x = lane_car_x(player_lane);
    for (uint8_t index = 0U; index < OBSTACLE_COUNT; ++index) {
        if (rectangles_overlap(
                player_x, PLAYER_Y, CAR_WIDTH, CAR_HEIGHT,
                lane_car_x(obstacles[index].lane), obstacles[index].y,
                CAR_WIDTH, CAR_HEIGHT)) {
            return true;
        }
    }
    return false;
}

void draw_overlay(const char *title, const char *line_two, const char *line_three)
{
    display_fill_rect(110, 116, 260, 94, COLOR_LCD);
    display_draw_rect(110, 116, 260, 94, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 130, title, 2U, COLOR_INK, COLOR_LCD);
    display_text(174, 166, line_two, 1U, COLOR_INK, COLOR_LCD);
    display_text(174, 186, line_three, 1U, COLOR_INK, COLOR_LCD);
}

}  // namespace

void racing_game_start(uint32_t now_ms)
{
    player_lane = 1U;
    dash_offset = 0U;
    score = 0UL;
    tick_interval_ms = INITIAL_TICK_MS;
    last_tick_ms = now_ms;
    for (uint8_t index = 0U; index < OBSTACLE_COUNT; ++index) {
        obstacles[index].lane = static_cast<uint8_t>(index % LANE_COUNT);
        obstacles[index].y = static_cast<int16_t>(
            ROAD_TOP - 38 - static_cast<int16_t>(index) * 76);
    }
    draw_complete_game();
}

RacingUpdateResult racing_game_update(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::LEFT && player_lane > 0U) {
        erase_car(lane_car_x(player_lane), PLAYER_Y);
        --player_lane;
        draw_player();
    } else if (event == InputEvent::RIGHT && player_lane + 1U < LANE_COUNT) {
        erase_car(lane_car_x(player_lane), PLAYER_Y);
        ++player_lane;
        draw_player();
    }

    if (player_hit_obstacle()) {
        return RacingUpdateResult::GAME_OVER;
    }

    if (static_cast<uint32_t>(now_ms - last_tick_ms) < tick_interval_ms) {
        return RacingUpdateResult::RUNNING;
    }
    last_tick_ms = now_ms;

    draw_dashes(dash_offset, COLOR_LCD_SHADE);
    erase_obstacles();
    dash_offset = static_cast<uint8_t>((dash_offset + SCROLL_STEP) % DASH_PERIOD);

    bool score_changed = false;
    for (uint8_t index = 0U; index < OBSTACLE_COUNT; ++index) {
        obstacles[index].y = static_cast<int16_t>(obstacles[index].y + SCROLL_STEP);
        if (obstacles[index].y >= ROAD_BOTTOM) {
            ++score;
            score_changed = true;
            respawn_obstacle(index);
        }
    }

    if (score_changed) {
        tick_interval_ms = score >= INITIAL_TICK_MS - MINIMUM_TICK_MS ?
            MINIMUM_TICK_MS : static_cast<uint16_t>(INITIAL_TICK_MS - score);
        draw_score();
    }
    draw_dashes(dash_offset, COLOR_INK_LIGHT);
    draw_obstacles();
    draw_player();

    return player_hit_obstacle() ?
        RacingUpdateResult::GAME_OVER : RacingUpdateResult::RUNNING;
}

void racing_game_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void racing_game_resume(uint32_t now_ms)
{
    last_tick_ms = now_ms;
    draw_complete_game();
}

void racing_game_show_game_over()
{
    char score_text[18] = "SCORE ";
    ultoa(score, &score_text[6], 10);
    draw_overlay("CRASH", score_text, "PRESS RESTART  HOLD MENU");
}
