#include "snake_game.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

struct Cell {
    uint8_t x;
    uint8_t y;
};

enum class Direction : uint8_t {
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

// The ring stores cells from tail to head. Advancing the tail and appending at
// (tail + length) % capacity moves the snake without copying its body.
Cell snake_cells[MAX_SNAKE_LENGTH];
uint16_t tail_index = 0U;
uint16_t snake_length = 0U;
Direction direction = Direction::RIGHT;
Direction requested_direction = Direction::RIGHT;
bool turn_pending = false;
Cell food = {0U, 0U};
uint16_t score = 0U;
uint16_t move_interval_ms = INITIAL_MOVE_INTERVAL_MS;
uint32_t last_move_ms = 0UL;

uint16_t ring_index(uint16_t logical_offset)
{
    return static_cast<uint16_t>((tail_index + logical_offset) % MAX_SNAKE_LENGTH);
}

Cell snake_cell(uint16_t logical_offset)
{
    return snake_cells[ring_index(logical_offset)];
}

Cell snake_head()
{
    return snake_cell(static_cast<uint16_t>(snake_length - 1U));
}

bool same_cell(const Cell &left, const Cell &right)
{
    return left.x == right.x && left.y == right.y;
}

bool cell_is_occupied(uint8_t x, uint8_t y, bool ignore_tail)
{
    const uint16_t first = ignore_tail ? 1U : 0U;
    for (uint16_t offset = first; offset < snake_length; ++offset) {
        const Cell cell = snake_cell(offset);
        if (cell.x == x && cell.y == y) {
            return true;
        }
    }
    return false;
}

bool directions_are_opposite(Direction first, Direction second)
{
    return (first == Direction::UP && second == Direction::DOWN) ||
           (first == Direction::DOWN && second == Direction::UP) ||
           (first == Direction::LEFT && second == Direction::RIGHT) ||
           (first == Direction::RIGHT && second == Direction::LEFT);
}

bool event_direction(InputEvent event, Direction *result)
{
    if (result == nullptr) {
        return false;
    }
    switch (event) {
        case InputEvent::UP: *result = Direction::UP; return true;
        case InputEvent::DOWN: *result = Direction::DOWN; return true;
        case InputEvent::LEFT: *result = Direction::LEFT; return true;
        case InputEvent::RIGHT: *result = Direction::RIGHT; return true;
        default: return false;
    }
}

int16_t cell_pixel_x(uint8_t x)
{
    return static_cast<int16_t>(PLAYFIELD_X + static_cast<int16_t>(x) * CELL_SIZE);
}

int16_t cell_pixel_y(uint8_t y)
{
    return static_cast<int16_t>(PLAYFIELD_Y + static_cast<int16_t>(y) * CELL_SIZE);
}

void draw_empty_cell(uint8_t x, uint8_t y)
{
    display_fill_rect(
        cell_pixel_x(x), cell_pixel_y(y), CELL_SIZE, CELL_SIZE, COLOR_LCD);
}

void draw_snake_body_cell(uint8_t x, uint8_t y)
{
    display_fill_rect(
        cell_pixel_x(x) + 2, cell_pixel_y(y) + 2,
        CELL_SIZE - 4, CELL_SIZE - 4, COLOR_INK);
}

void draw_snake_head_cell(uint8_t x, uint8_t y)
{
    display_fill_rect(
        cell_pixel_x(x) + 1, cell_pixel_y(y) + 1,
        CELL_SIZE - 2, CELL_SIZE - 2, COLOR_INK);
    display_fill_rect(
        cell_pixel_x(x) + 5, cell_pixel_y(y) + 5, 4, 4, COLOR_LCD);
}

void draw_food_cell(uint8_t x, uint8_t y)
{
    display_draw_rect(
        cell_pixel_x(x) + 2, cell_pixel_y(y) + 2,
        CELL_SIZE - 4, CELL_SIZE - 4, COLOR_INK);
    display_fill_rect(
        cell_pixel_x(x) + 5, cell_pixel_y(y) + 5, 4, 4, COLOR_INK);
}

void format_score(char *output)
{
    output[0] = 'S';
    output[1] = 'C';
    output[2] = 'O';
    output[3] = 'R';
    output[4] = 'E';
    output[5] = ' ';
    output[6] = static_cast<char>('0' + ((score / 100U) % 10U));
    output[7] = static_cast<char>('0' + ((score / 10U) % 10U));
    output[8] = static_cast<char>('0' + (score % 10U));
    output[9] = '\0';
}

void draw_score()
{
    char score_text[10];
    format_score(score_text);
    display_fill_rect(298, 30, 126, 20, COLOR_LCD);
    display_text(304, 32, score_text, 2U, COLOR_INK, COLOR_LCD);
}

void draw_playfield()
{
    display_fill_rect(
        PLAYFIELD_X, PLAYFIELD_Y, PLAYFIELD_WIDTH, PLAYFIELD_HEIGHT, COLOR_LCD);
    display_draw_rect(
        PLAYFIELD_X - 2, PLAYFIELD_Y - 2,
        PLAYFIELD_WIDTH + 4, PLAYFIELD_HEIGHT + 4, COLOR_INK);
    display_draw_rect(
        PLAYFIELD_X - 4, PLAYFIELD_Y - 4,
        PLAYFIELD_WIDTH + 8, PLAYFIELD_HEIGHT + 8, COLOR_LCD_SHADE);
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(48, 32, "SNAKE", 2U, COLOR_INK, COLOR_LCD);
    draw_score();
    draw_playfield();
    for (uint16_t offset = 0U; offset + 1U < snake_length; ++offset) {
        const Cell cell = snake_cell(offset);
        draw_snake_body_cell(cell.x, cell.y);
    }
    const Cell head = snake_head();
    draw_snake_head_cell(head.x, head.y);
    draw_food_cell(food.x, food.y);
}

bool place_food()
{
    if (snake_length >= MAX_SNAKE_LENGTH) {
        return false;
    }

    for (uint8_t attempt = 0U; attempt < FOOD_RANDOM_ATTEMPTS; ++attempt) {
        const uint8_t x = static_cast<uint8_t>(random(SNAKE_COLS));
        const uint8_t y = static_cast<uint8_t>(random(SNAKE_ROWS));
        if (!cell_is_occupied(x, y, false)) {
            food = {x, y};
            return true;
        }
    }

    // Bounded deterministic fallback guarantees termination near a full board.
    for (uint8_t y = 0U; y < SNAKE_ROWS; ++y) {
        for (uint8_t x = 0U; x < SNAKE_COLS; ++x) {
            if (!cell_is_occupied(x, y, false)) {
                food = {x, y};
                return true;
            }
        }
    }
    return false;
}

Cell next_head_cell(const Cell &head)
{
    Cell next = head;
    if (direction == Direction::UP) {
        --next.y;
    } else if (direction == Direction::DOWN) {
        ++next.y;
    } else if (direction == Direction::LEFT) {
        --next.x;
    } else {
        ++next.x;
    }
    return next;
}

void draw_overlay(const char *title, const char *line_two, const char *line_three)
{
    display_fill_rect(116, 118, 248, 92, COLOR_LCD_SHADE);
    display_draw_rect(116, 118, 248, 92, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 132, title, 2U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(174, 166, line_two, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(174, 186, line_three, 1U, COLOR_INK, COLOR_LCD_SHADE);
}

}  // namespace

void snake_game_start(uint32_t now_ms)
{
    tail_index = 0U;
    snake_length = INITIAL_SNAKE_LENGTH;
    direction = Direction::RIGHT;
    requested_direction = Direction::RIGHT;
    turn_pending = false;
    score = 0U;
    move_interval_ms = INITIAL_MOVE_INTERVAL_MS;
    last_move_ms = now_ms;

    const uint8_t center_y = static_cast<uint8_t>(SNAKE_ROWS / 2U);
    const uint8_t head_x = static_cast<uint8_t>(SNAKE_COLS / 2U);
    for (uint8_t index = 0U; index < INITIAL_SNAKE_LENGTH; ++index) {
        snake_cells[index] = {
            static_cast<uint8_t>(head_x - INITIAL_SNAKE_LENGTH + 1U + index),
            center_y,
        };
    }
    place_food();
    draw_complete_game();
}

void snake_game_handle_direction(InputEvent event)
{
    Direction candidate = direction;
    if (turn_pending || !event_direction(event, &candidate) ||
        candidate == direction || directions_are_opposite(candidate, direction)) {
        return;
    }
    requested_direction = candidate;
    turn_pending = true;
}

SnakeTickResult snake_game_update(uint32_t now_ms)
{
    if (static_cast<uint32_t>(now_ms - last_move_ms) < move_interval_ms) {
        return SnakeTickResult::RUNNING;
    }
    last_move_ms = now_ms;

    direction = requested_direction;
    turn_pending = false;
    requested_direction = direction;

    const Cell previous_head = snake_head();
    const Cell next_head = next_head_cell(previous_head);
    if (next_head.x >= SNAKE_COLS || next_head.y >= SNAKE_ROWS) {
        return SnakeTickResult::LOST;
    }

    const bool eating = same_cell(next_head, food);
    if (cell_is_occupied(next_head.x, next_head.y, !eating)) {
        return SnakeTickResult::LOST;
    }

    const uint16_t insertion_index = ring_index(snake_length);
    Cell previous_tail = snake_cells[tail_index];
    if (eating) {
        ++snake_length;
        ++score;
        if (move_interval_ms > MINIMUM_MOVE_INTERVAL_MS + MOVE_INTERVAL_STEP_MS) {
            move_interval_ms = static_cast<uint16_t>(
                move_interval_ms - MOVE_INTERVAL_STEP_MS);
        } else {
            move_interval_ms = MINIMUM_MOVE_INTERVAL_MS;
        }
    } else {
        tail_index = static_cast<uint16_t>((tail_index + 1U) % MAX_SNAKE_LENGTH);
    }
    snake_cells[insertion_index] = next_head;

    draw_snake_body_cell(previous_head.x, previous_head.y);
    if (!eating) {
        draw_empty_cell(previous_tail.x, previous_tail.y);
    }
    draw_snake_head_cell(next_head.x, next_head.y);

    if (eating) {
        draw_score();
        if (snake_length >= MAX_SNAKE_LENGTH || !place_food()) {
            return SnakeTickResult::WON;
        }
        draw_food_cell(food.x, food.y);
    }
    return SnakeTickResult::RUNNING;
}

void snake_game_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void snake_game_resume(uint32_t now_ms)
{
    last_move_ms = now_ms;
    requested_direction = direction;
    turn_pending = false;
    draw_complete_game();
}

void snake_game_show_result(bool won)
{
    char score_text[10];
    format_score(score_text);
    draw_overlay(won ? "YOU WIN" : "GAME OVER", score_text,
                 "PRESS RESTART  HOLD MENU");
}

uint16_t snake_game_score()
{
    return score;
}
