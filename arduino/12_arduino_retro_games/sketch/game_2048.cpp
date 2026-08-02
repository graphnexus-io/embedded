#include "game_2048.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

constexpr uint8_t BOARD_SIZE = 4U;
constexpr int16_t BOARD_X = 116;
constexpr int16_t BOARD_Y = 58;
constexpr int16_t TILE_SIZE = 58;
constexpr int16_t TILE_PITCH = 62;
constexpr uint16_t WINNING_TILE = 2048U;

uint16_t board[BOARD_SIZE][BOARD_SIZE];
uint32_t score = 0UL;

uint16_t read_line_cell(InputEvent direction, uint8_t line, uint8_t position)
{
    switch (direction) {
        case InputEvent::LEFT: return board[line][position];
        case InputEvent::RIGHT:
            return board[line][static_cast<uint8_t>(BOARD_SIZE - 1U - position)];
        case InputEvent::UP: return board[position][line];
        case InputEvent::DOWN:
            return board[static_cast<uint8_t>(BOARD_SIZE - 1U - position)][line];
        default: return 0U;
    }
}

void write_line_cell(
    InputEvent direction, uint8_t line, uint8_t position, uint16_t value)
{
    switch (direction) {
        case InputEvent::LEFT:
            board[line][position] = value;
            break;
        case InputEvent::RIGHT:
            board[line][static_cast<uint8_t>(BOARD_SIZE - 1U - position)] = value;
            break;
        case InputEvent::UP:
            board[position][line] = value;
            break;
        case InputEvent::DOWN:
            board[static_cast<uint8_t>(BOARD_SIZE - 1U - position)][line] = value;
            break;
        default:
            break;
    }
}

bool slide_line(InputEvent direction, uint8_t line)
{
    uint16_t compacted[BOARD_SIZE] = {0U, 0U, 0U, 0U};
    uint16_t result[BOARD_SIZE] = {0U, 0U, 0U, 0U};
    uint8_t compacted_count = 0U;
    for (uint8_t position = 0U; position < BOARD_SIZE; ++position) {
        const uint16_t value = read_line_cell(direction, line, position);
        if (value != 0U) {
            compacted[compacted_count++] = value;
        }
    }

    uint8_t result_count = 0U;
    for (uint8_t source = 0U; source < compacted_count; ++source) {
        uint16_t value = compacted[source];
        if (source + 1U < compacted_count && value == compacted[source + 1U]) {
            if (value <= 32767U) {
                value = static_cast<uint16_t>(value * 2U);
            }
            score += value;
            ++source;
        }
        result[result_count++] = value;
    }

    bool changed = false;
    for (uint8_t position = 0U; position < BOARD_SIZE; ++position) {
        if (read_line_cell(direction, line, position) != result[position]) {
            changed = true;
        }
        write_line_cell(direction, line, position, result[position]);
    }
    return changed;
}

bool spawn_tile()
{
    uint8_t empty_count = 0U;
    for (uint8_t y = 0U; y < BOARD_SIZE; ++y) {
        for (uint8_t x = 0U; x < BOARD_SIZE; ++x) {
            if (board[y][x] == 0U) {
                ++empty_count;
            }
        }
    }
    if (empty_count == 0U) {
        return false;
    }

    uint8_t target = static_cast<uint8_t>(random(empty_count));
    for (uint8_t y = 0U; y < BOARD_SIZE; ++y) {
        for (uint8_t x = 0U; x < BOARD_SIZE; ++x) {
            if (board[y][x] != 0U) {
                continue;
            }
            if (target == 0U) {
                board[y][x] = random(10L) == 0L ? 4U : 2U;
                return true;
            }
            --target;
        }
    }
    return false;
}

bool has_winning_tile()
{
    for (uint8_t y = 0U; y < BOARD_SIZE; ++y) {
        for (uint8_t x = 0U; x < BOARD_SIZE; ++x) {
            if (board[y][x] >= WINNING_TILE) {
                return true;
            }
        }
    }
    return false;
}

bool has_available_move()
{
    for (uint8_t y = 0U; y < BOARD_SIZE; ++y) {
        for (uint8_t x = 0U; x < BOARD_SIZE; ++x) {
            if (board[y][x] == 0U ||
                (x + 1U < BOARD_SIZE && board[y][x] == board[y][x + 1U]) ||
                (y + 1U < BOARD_SIZE && board[y][x] == board[y + 1U][x])) {
                return true;
            }
        }
    }
    return false;
}

void draw_score()
{
    char text[20] = "SCORE ";
    ultoa(score, &text[6], 10);
    display_fill_rect(256, 28, 176, 20, COLOR_LCD);
    display_text(262, 30, text, 2U, COLOR_INK, COLOR_LCD);
}

void draw_tile(uint8_t x, uint8_t y)
{
    const int16_t pixel_x = static_cast<int16_t>(BOARD_X + x * TILE_PITCH);
    const int16_t pixel_y = static_cast<int16_t>(BOARD_Y + y * TILE_PITCH);
    const uint16_t value = board[y][x];
    display_fill_rect(pixel_x, pixel_y, TILE_SIZE, TILE_SIZE, COLOR_LCD);
    display_draw_rect(pixel_x, pixel_y, TILE_SIZE, TILE_SIZE, COLOR_INK_LIGHT);
    if (value == 0U) {
        display_fill_rect(pixel_x + 5, pixel_y + 5, TILE_SIZE - 10, TILE_SIZE - 10,
                          COLOR_LCD_SHADE);
        return;
    }

    uint8_t exponent = 0U;
    for (uint16_t copy = value; copy > 1U; copy >>= 1U) {
        ++exponent;
    }
    const uint16_t background =
        exponent % 2U == 0U ? COLOR_LCD_SHADE : COLOR_INK_LIGHT;
    display_fill_rect(pixel_x + 4, pixel_y + 4, TILE_SIZE - 8, TILE_SIZE - 8,
                      background);

    char number[6];
    utoa(value, number, 10);
    const uint8_t scale = value < 10000U ? 2U : 1U;
    const int16_t text_width = static_cast<int16_t>(strlen(number) * 6U * scale);
    const int16_t text_height = static_cast<int16_t>(8U * scale);
    display_text(
        static_cast<int16_t>(pixel_x + (TILE_SIZE - text_width) / 2),
        static_cast<int16_t>(pixel_y + (TILE_SIZE - text_height) / 2),
        number, scale, COLOR_INK, background);
}

void draw_board()
{
    display_fill_rect(
        BOARD_X - 4, BOARD_Y - 4,
        BOARD_SIZE * TILE_PITCH, BOARD_SIZE * TILE_PITCH, COLOR_LCD);
    for (uint8_t y = 0U; y < BOARD_SIZE; ++y) {
        for (uint8_t x = 0U; x < BOARD_SIZE; ++x) {
            draw_tile(x, y);
        }
    }
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(54, 30, "2048", 2U, COLOR_INK, COLOR_LCD);
    draw_score();
    draw_board();
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

void game_2048_start()
{
    memset(board, 0, sizeof(board));
    score = 0UL;
    spawn_tile();
    spawn_tile();
    draw_complete_game();
}

Game2048UpdateResult game_2048_update(InputEvent event)
{
    if (event != InputEvent::UP && event != InputEvent::DOWN &&
        event != InputEvent::LEFT && event != InputEvent::RIGHT) {
        return Game2048UpdateResult::RUNNING;
    }

    bool changed = false;
    for (uint8_t line = 0U; line < BOARD_SIZE; ++line) {
        if (slide_line(event, line)) {
            changed = true;
        }
    }
    if (changed) {
        spawn_tile();
        draw_score();
        draw_board();
        if (has_winning_tile()) {
            return Game2048UpdateResult::WON;
        }
    }
    return has_available_move() ?
        Game2048UpdateResult::RUNNING : Game2048UpdateResult::LOST;
}

void game_2048_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void game_2048_resume()
{
    draw_complete_game();
}

void game_2048_show_result(bool won)
{
    char score_text[20] = "SCORE ";
    ultoa(score, &score_text[6], 10);
    draw_overlay(won ? "YOU MADE 2048" : "NO MOVES", score_text,
                 "PRESS RESTART  HOLD MENU");
}
