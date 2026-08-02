#include "breakout_game.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

bool bricks[BREAKOUT_BRICK_ROWS][BREAKOUT_BRICK_COLS];
uint8_t remaining_bricks = 0U;
uint8_t lives = BREAKOUT_INITIAL_LIVES;
uint16_t score = 0U;
int16_t paddle_x = 0;
int16_t ball_x = 0;
int16_t ball_y = 0;
int8_t ball_dx = 2;
int8_t ball_dy = -2;
bool ball_launched = false;
uint32_t last_ball_ms = 0UL;

int16_t brick_x(uint8_t column)
{
    return static_cast<int16_t>(
        BREAKOUT_BRICK_START_X + column * BREAKOUT_BRICK_PITCH_X);
}

int16_t brick_y(uint8_t row)
{
    return static_cast<int16_t>(
        BREAKOUT_BRICK_START_Y + row * BREAKOUT_BRICK_PITCH_Y);
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

void draw_paddle()
{
    display_fill_rect(
        paddle_x, BREAKOUT_PADDLE_Y,
        BREAKOUT_PADDLE_WIDTH, BREAKOUT_PADDLE_HEIGHT, COLOR_INK);
    display_fill_rect(
        paddle_x + 4, BREAKOUT_PADDLE_Y + 2,
        BREAKOUT_PADDLE_WIDTH - 8, BREAKOUT_PADDLE_HEIGHT - 4, COLOR_INK_LIGHT);
}

void erase_paddle()
{
    display_fill_rect(
        paddle_x, BREAKOUT_PADDLE_Y,
        BREAKOUT_PADDLE_WIDTH, BREAKOUT_PADDLE_HEIGHT, COLOR_LCD);
}

void draw_ball()
{
    display_fill_rect(
        ball_x, ball_y, BREAKOUT_BALL_SIZE, BREAKOUT_BALL_SIZE, COLOR_INK);
}

void erase_ball()
{
    display_fill_rect(
        ball_x, ball_y, BREAKOUT_BALL_SIZE, BREAKOUT_BALL_SIZE, COLOR_LCD);
}

void draw_brick(uint8_t row, uint8_t column)
{
    const int16_t x = brick_x(column);
    const int16_t y = brick_y(row);
    display_fill_rect(
        x, y, BREAKOUT_BRICK_WIDTH, BREAKOUT_BRICK_HEIGHT, COLOR_INK);
    display_fill_rect(
        x + 2, y + 2,
        BREAKOUT_BRICK_WIDTH - 4, BREAKOUT_BRICK_HEIGHT - 4,
        row % 2U == 0U ? COLOR_LCD_SHADE : COLOR_INK_LIGHT);
}

void erase_brick(uint8_t row, uint8_t column)
{
    display_fill_rect(
        brick_x(column), brick_y(row),
        BREAKOUT_BRICK_WIDTH, BREAKOUT_BRICK_HEIGHT, COLOR_LCD);
}

void format_score(char *output)
{
    output[0] = 'S';
    output[1] = 'C';
    output[2] = 'O';
    output[3] = 'R';
    output[4] = 'E';
    output[5] = ' ';
    ultoa(score, &output[6], 10);
}

void draw_score()
{
    char text[18];
    format_score(text);
    display_fill_rect(188, 28, 150, 18, COLOR_LCD);
    display_text(194, 30, text, 2U, COLOR_INK, COLOR_LCD);
}

void draw_lives()
{
    char text[9] = "LIVES 0";
    text[6] = static_cast<char>('0' + lives);
    display_fill_rect(352, 28, 82, 18, COLOR_LCD);
    display_text(356, 30, text, 1U, COLOR_INK, COLOR_LCD);
}

void clear_launch_prompt()
{
    display_fill_rect(180, 238, 120, 10, COLOR_LCD);
}

void draw_launch_prompt()
{
    clear_launch_prompt();
    display_text(195, 238, "ENTER - LAUNCH", 1U, COLOR_INK_LIGHT, COLOR_LCD);
}

void reset_ball(uint32_t now_ms)
{
    paddle_x = static_cast<int16_t>(
        BREAKOUT_FIELD_X + (BREAKOUT_FIELD_WIDTH - BREAKOUT_PADDLE_WIDTH) / 2);
    ball_x = static_cast<int16_t>(
        paddle_x + (BREAKOUT_PADDLE_WIDTH - BREAKOUT_BALL_SIZE) / 2);
    ball_y = static_cast<int16_t>(
        BREAKOUT_PADDLE_Y - BREAKOUT_BALL_SIZE - 2);
    ball_dx = random(2U) == 0L ? -2 : 2;
    ball_dy = -2;
    ball_launched = false;
    last_ball_ms = now_ms;
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(50, 30, "BREAKOUT", 2U, COLOR_INK, COLOR_LCD);
    draw_score();
    draw_lives();
    display_fill_rect(
        BREAKOUT_FIELD_X, BREAKOUT_FIELD_Y,
        BREAKOUT_FIELD_WIDTH, BREAKOUT_FIELD_HEIGHT, COLOR_LCD);
    display_draw_rect(
        BREAKOUT_FIELD_X - 2, BREAKOUT_FIELD_Y - 2,
        BREAKOUT_FIELD_WIDTH + 4, BREAKOUT_FIELD_HEIGHT + 4, COLOR_INK);
    display_draw_rect(
        BREAKOUT_FIELD_X - 4, BREAKOUT_FIELD_Y - 4,
        BREAKOUT_FIELD_WIDTH + 8, BREAKOUT_FIELD_HEIGHT + 8, COLOR_LCD_SHADE);
    for (uint8_t row = 0U; row < BREAKOUT_BRICK_ROWS; ++row) {
        for (uint8_t column = 0U; column < BREAKOUT_BRICK_COLS; ++column) {
            if (bricks[row][column]) {
                draw_brick(row, column);
            }
        }
    }
    draw_paddle();
    draw_ball();
    if (!ball_launched) {
        draw_launch_prompt();
    }
}

void move_paddle(int8_t direction)
{
    if (!ball_launched) {
        erase_ball();
    }
    erase_paddle();

    const int16_t left_limit = static_cast<int16_t>(BREAKOUT_FIELD_X + 3);
    const int16_t right_limit = static_cast<int16_t>(
        BREAKOUT_FIELD_X + BREAKOUT_FIELD_WIDTH - 3 - BREAKOUT_PADDLE_WIDTH);
    int16_t next = static_cast<int16_t>(
        paddle_x + static_cast<int16_t>(direction) * BREAKOUT_PADDLE_STEP);
    if (next < left_limit) {
        next = left_limit;
    } else if (next > right_limit) {
        next = right_limit;
    }
    paddle_x = next;
    if (!ball_launched) {
        ball_x = static_cast<int16_t>(
            paddle_x + (BREAKOUT_PADDLE_WIDTH - BREAKOUT_BALL_SIZE) / 2);
    }
    draw_paddle();
    if (!ball_launched) {
        draw_ball();
    }
}

bool hit_brick(int16_t next_x, int16_t next_y)
{
    for (uint8_t row = 0U; row < BREAKOUT_BRICK_ROWS; ++row) {
        for (uint8_t column = 0U; column < BREAKOUT_BRICK_COLS; ++column) {
            if (!bricks[row][column]) {
                continue;
            }
            const int16_t target_x = brick_x(column);
            const int16_t target_y = brick_y(row);
            if (!rectangles_overlap(
                    next_x, next_y, BREAKOUT_BALL_SIZE, BREAKOUT_BALL_SIZE,
                    target_x, target_y,
                    BREAKOUT_BRICK_WIDTH, BREAKOUT_BRICK_HEIGHT)) {
                continue;
            }

            bricks[row][column] = false;
            --remaining_bricks;
            score = static_cast<uint16_t>(score +
                static_cast<uint16_t>(BREAKOUT_BRICK_ROWS - row) * 10U);
            erase_brick(row, column);
            draw_score();

            const bool vertical_contact =
                ball_y + BREAKOUT_BALL_SIZE <= target_y ||
                ball_y >= target_y + BREAKOUT_BRICK_HEIGHT;
            if (vertical_contact) {
                ball_dy = static_cast<int8_t>(-ball_dy);
            } else {
                ball_dx = static_cast<int8_t>(-ball_dx);
            }
            return true;
        }
    }
    return false;
}

BreakoutUpdateResult lose_ball(uint32_t now_ms)
{
    if (lives > 0U) {
        --lives;
    }
    draw_lives();
    if (lives == 0U) {
        return BreakoutUpdateResult::LOST;
    }
    erase_paddle();
    reset_ball(now_ms);
    draw_paddle();
    draw_ball();
    draw_launch_prompt();
    return BreakoutUpdateResult::RUNNING;
}

BreakoutUpdateResult advance_ball(uint32_t now_ms)
{
    erase_ball();

    int16_t next_x = static_cast<int16_t>(ball_x + ball_dx);
    int16_t next_y = static_cast<int16_t>(ball_y + ball_dy);
    const int16_t left = static_cast<int16_t>(BREAKOUT_FIELD_X + 3);
    const int16_t right = static_cast<int16_t>(
        BREAKOUT_FIELD_X + BREAKOUT_FIELD_WIDTH - 4);
    const int16_t top = static_cast<int16_t>(BREAKOUT_FIELD_Y + 3);
    const int16_t bottom = static_cast<int16_t>(
        BREAKOUT_FIELD_Y + BREAKOUT_FIELD_HEIGHT - 3);

    if (next_x < left) {
        next_x = left;
        ball_dx = static_cast<int8_t>(abs(ball_dx));
    } else if (next_x + BREAKOUT_BALL_SIZE - 1 > right) {
        next_x = static_cast<int16_t>(right - BREAKOUT_BALL_SIZE + 1);
        ball_dx = static_cast<int8_t>(-abs(ball_dx));
    }
    if (next_y < top) {
        next_y = top;
        ball_dy = static_cast<int8_t>(abs(ball_dy));
    }

    if (ball_dy > 0 && rectangles_overlap(
            next_x, next_y, BREAKOUT_BALL_SIZE, BREAKOUT_BALL_SIZE,
            paddle_x, BREAKOUT_PADDLE_Y,
            BREAKOUT_PADDLE_WIDTH, BREAKOUT_PADDLE_HEIGHT)) {
        next_y = static_cast<int16_t>(BREAKOUT_PADDLE_Y - BREAKOUT_BALL_SIZE);
        ball_dy = static_cast<int8_t>(-abs(ball_dy));
        const int16_t hit = static_cast<int16_t>(
            next_x + BREAKOUT_BALL_SIZE / 2 - paddle_x);
        if (hit < BREAKOUT_PADDLE_WIDTH / 4) {
            ball_dx = -3;
        } else if (hit < BREAKOUT_PADDLE_WIDTH / 2) {
            ball_dx = -2;
        } else if (hit < (BREAKOUT_PADDLE_WIDTH * 3) / 4) {
            ball_dx = 2;
        } else {
            ball_dx = 3;
        }
    }

    if (hit_brick(next_x, next_y)) {
        next_x = static_cast<int16_t>(ball_x + ball_dx);
        next_y = static_cast<int16_t>(ball_y + ball_dy);
        if (remaining_bricks == 0U) {
            return BreakoutUpdateResult::WON;
        }
    }

    if (next_y + BREAKOUT_BALL_SIZE > bottom) {
        return lose_ball(now_ms);
    }

    ball_x = next_x;
    ball_y = next_y;
    draw_ball();
    last_ball_ms = now_ms;
    return BreakoutUpdateResult::RUNNING;
}

void draw_overlay(const char *title, const char *line_two, const char *line_three)
{
    display_fill_rect(110, 116, 260, 94, COLOR_LCD_SHADE);
    display_draw_rect(110, 116, 260, 94, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 130, title, 2U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(174, 166, line_two, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(174, 186, line_three, 1U, COLOR_INK, COLOR_LCD_SHADE);
}

}  // namespace

void breakout_game_start(uint32_t now_ms)
{
    for (uint8_t row = 0U; row < BREAKOUT_BRICK_ROWS; ++row) {
        for (uint8_t column = 0U; column < BREAKOUT_BRICK_COLS; ++column) {
            bricks[row][column] = true;
        }
    }
    remaining_bricks = BREAKOUT_BRICK_ROWS * BREAKOUT_BRICK_COLS;
    lives = BREAKOUT_INITIAL_LIVES;
    score = 0U;
    reset_ball(now_ms);
    draw_complete_game();
}

BreakoutUpdateResult breakout_game_update(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::LEFT) {
        move_paddle(-1);
    } else if (event == InputEvent::RIGHT) {
        move_paddle(1);
    } else if ((event == InputEvent::SELECT || event == InputEvent::UP) &&
               !ball_launched) {
        ball_launched = true;
        last_ball_ms = now_ms;
        clear_launch_prompt();
    }

    if (ball_launched &&
        static_cast<uint32_t>(now_ms - last_ball_ms) >= BREAKOUT_BALL_INTERVAL_MS) {
        return advance_ball(now_ms);
    }
    return BreakoutUpdateResult::RUNNING;
}

void breakout_game_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void breakout_game_resume(uint32_t now_ms)
{
    last_ball_ms = now_ms;
    draw_complete_game();
}

void breakout_game_show_result(bool won)
{
    char score_text[18];
    format_score(score_text);
    draw_overlay(won ? "YOU WIN" : "GAME OVER", score_text,
                 "PRESS RESTART  HOLD MENU");
}
