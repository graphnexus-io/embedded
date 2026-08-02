#include "tetris_game.h"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

enum Tetromino : uint8_t {
    PIECE_I = 0U,
    PIECE_O,
    PIECE_T,
    PIECE_S,
    PIECE_Z,
    PIECE_J,
    PIECE_L,
    PIECE_COUNT,
};

// Each rotation is a 4x4 bitmap. Bit (row * 4 + column) represents one block.
const uint16_t piece_masks[PIECE_COUNT][4] PROGMEM = {
    {0x00F0U, 0x4444U, 0x0F00U, 0x2222U},  // I
    {0x0660U, 0x0660U, 0x0660U, 0x0660U},  // O
    {0x0072U, 0x0262U, 0x0270U, 0x0232U},  // T
    {0x0036U, 0x0462U, 0x0360U, 0x0231U},  // S
    {0x0063U, 0x0264U, 0x0630U, 0x0132U},  // Z
    {0x0071U, 0x0226U, 0x0470U, 0x0322U},  // J
    {0x0074U, 0x0622U, 0x0170U, 0x0223U},  // L
};

uint8_t settled[TETRIS_ROWS][TETRIS_COLS];
uint8_t active_piece = PIECE_I;
uint8_t active_rotation = 0U;
int8_t active_x = 3;
int8_t active_y = -1;
uint8_t next_piece = PIECE_O;
uint8_t bag[PIECE_COUNT];
uint8_t bag_position = PIECE_COUNT;
uint32_t score = 0UL;
uint16_t cleared_lines = 0U;
uint8_t level = 1U;
uint16_t fall_interval_ms = TETRIS_INITIAL_FALL_MS;
uint32_t last_fall_ms = 0UL;

uint16_t active_mask(uint8_t piece, uint8_t rotation)
{
    return pgm_read_word(&piece_masks[piece][rotation & 0x03U]);
}

bool mask_has_block(uint16_t mask, uint8_t x, uint8_t y)
{
    const uint8_t bit = static_cast<uint8_t>(y * 4U + x);
    return (mask & (static_cast<uint16_t>(1U) << bit)) != 0U;
}

void refill_bag()
{
    for (uint8_t index = 0U; index < PIECE_COUNT; ++index) {
        bag[index] = index;
    }
    for (int8_t index = static_cast<int8_t>(PIECE_COUNT - 1U); index > 0; --index) {
        const uint8_t other = static_cast<uint8_t>(random(index + 1));
        const uint8_t temporary = bag[static_cast<uint8_t>(index)];
        bag[static_cast<uint8_t>(index)] = bag[other];
        bag[other] = temporary;
    }
    bag_position = 0U;
}

uint8_t take_piece_from_bag()
{
    if (bag_position >= PIECE_COUNT) {
        refill_bag();
    }
    return bag[bag_position++];
}

bool piece_fits(uint8_t piece, uint8_t rotation, int8_t origin_x, int8_t origin_y)
{
    const uint16_t mask = active_mask(piece, rotation);
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            if (!mask_has_block(mask, x, y)) {
                continue;
            }
            const int8_t board_x = static_cast<int8_t>(origin_x + x);
            const int8_t board_y = static_cast<int8_t>(origin_y + y);
            if (board_x < 0 || board_x >= static_cast<int8_t>(TETRIS_COLS) ||
                board_y >= static_cast<int8_t>(TETRIS_ROWS)) {
                return false;
            }
            if (board_y >= 0 && settled[board_y][board_x] != 0U) {
                return false;
            }
        }
    }
    return true;
}

int16_t block_pixel_x(uint8_t x)
{
    return static_cast<int16_t>(TETRIS_BOARD_X +
        static_cast<int16_t>(x) * TETRIS_CELL_SIZE);
}

int16_t block_pixel_y(uint8_t y)
{
    return static_cast<int16_t>(TETRIS_BOARD_Y +
        static_cast<int16_t>(y) * TETRIS_CELL_SIZE);
}

void draw_empty_block(uint8_t x, uint8_t y)
{
    display_fill_rect(
        block_pixel_x(x), block_pixel_y(y),
        TETRIS_CELL_SIZE, TETRIS_CELL_SIZE, COLOR_LCD);
}

void draw_settled_block(uint8_t x, uint8_t y)
{
    display_fill_rect(
        block_pixel_x(x) + 2, block_pixel_y(y) + 2,
        TETRIS_CELL_SIZE - 4, TETRIS_CELL_SIZE - 4, COLOR_INK);
}

void draw_active_block(uint8_t x, uint8_t y)
{
    display_fill_rect(
        block_pixel_x(x) + 1, block_pixel_y(y) + 1,
        TETRIS_CELL_SIZE - 2, TETRIS_CELL_SIZE - 2, COLOR_INK);
    display_fill_rect(
        block_pixel_x(x) + 4, block_pixel_y(y) + 4,
        TETRIS_CELL_SIZE - 8, TETRIS_CELL_SIZE - 8, COLOR_LCD_SHADE);
}

void draw_board_cell(uint8_t x, uint8_t y)
{
    draw_empty_block(x, y);
    if (settled[y][x] != 0U) {
        draw_settled_block(x, y);
    }
}

void draw_piece(uint8_t piece, uint8_t rotation, int8_t origin_x, int8_t origin_y, bool active)
{
    const uint16_t mask = active_mask(piece, rotation);
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            if (!mask_has_block(mask, x, y)) {
                continue;
            }
            const int8_t board_x = static_cast<int8_t>(origin_x + x);
            const int8_t board_y = static_cast<int8_t>(origin_y + y);
            if (board_x < 0 || board_x >= static_cast<int8_t>(TETRIS_COLS) ||
                board_y < 0 || board_y >= static_cast<int8_t>(TETRIS_ROWS)) {
                continue;
            }
            if (active) {
                draw_active_block(static_cast<uint8_t>(board_x), static_cast<uint8_t>(board_y));
            } else {
                draw_board_cell(static_cast<uint8_t>(board_x), static_cast<uint8_t>(board_y));
            }
        }
    }
}

void erase_active_piece()
{
    draw_piece(active_piece, active_rotation, active_x, active_y, false);
}

void draw_active_piece()
{
    draw_piece(active_piece, active_rotation, active_x, active_y, true);
}

void format_u32(uint32_t value, char *output)
{
    ultoa(value, output, 10);
}

void draw_stat(const char *label, uint32_t value, int16_t y)
{
    char number[11];
    format_u32(value, number);
    display_fill_rect(244, y, 178, 36, COLOR_LCD);
    display_text(248, y, label, 1U, COLOR_INK_LIGHT, COLOR_LCD);
    display_text(248, y + 13, number, 2U, COLOR_INK, COLOR_LCD);
}

void draw_statistics()
{
    draw_stat("SCORE", score, 72);
    draw_stat("LINES", cleared_lines, 116);
    draw_stat("LEVEL", level, 160);
}

void draw_next_piece()
{
    display_fill_rect(244, 210, 94, 72, COLOR_LCD);
    display_text(248, 210, "NEXT", 1U, COLOR_INK_LIGHT, COLOR_LCD);
    const uint16_t mask = active_mask(next_piece, 0U);
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            if (mask_has_block(mask, x, y)) {
                display_fill_rect(
                    static_cast<int16_t>(254 + x * 11U),
                    static_cast<int16_t>(228 + y * 11U),
                    9, 9, COLOR_INK);
            }
        }
    }
}

void draw_board()
{
    display_fill_rect(
        TETRIS_BOARD_X, TETRIS_BOARD_Y,
        TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT, COLOR_LCD);
    display_draw_rect(
        TETRIS_BOARD_X - 2, TETRIS_BOARD_Y - 2,
        TETRIS_BOARD_WIDTH + 4, TETRIS_BOARD_HEIGHT + 4, COLOR_INK);
    display_draw_rect(
        TETRIS_BOARD_X - 4, TETRIS_BOARD_Y - 4,
        TETRIS_BOARD_WIDTH + 8, TETRIS_BOARD_HEIGHT + 8, COLOR_LCD_SHADE);
    for (uint8_t y = 0U; y < TETRIS_ROWS; ++y) {
        for (uint8_t x = 0U; x < TETRIS_COLS; ++x) {
            if (settled[y][x] != 0U) {
                draw_settled_block(x, y);
            }
        }
    }
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(246, 34, "TETRIS", 2U, COLOR_INK, COLOR_LCD);
    draw_board();
    draw_statistics();
    draw_next_piece();
    draw_active_piece();
}

bool spawn_piece(uint32_t now_ms)
{
    active_piece = next_piece;
    next_piece = take_piece_from_bag();
    active_rotation = 0U;
    active_x = 3;
    active_y = -1;
    last_fall_ms = now_ms;
    draw_next_piece();
    if (!piece_fits(active_piece, active_rotation, active_x, active_y)) {
        return false;
    }
    draw_active_piece();
    return true;
}

uint8_t clear_complete_lines()
{
    uint8_t removed = 0U;
    int8_t row = static_cast<int8_t>(TETRIS_ROWS - 1U);
    while (row >= 0) {
        bool full = true;
        for (uint8_t x = 0U; x < TETRIS_COLS; ++x) {
            if (settled[static_cast<uint8_t>(row)][x] == 0U) {
                full = false;
                break;
            }
        }
        if (!full) {
            --row;
            continue;
        }

        for (int8_t destination = row; destination > 0; --destination) {
            memcpy(
                settled[static_cast<uint8_t>(destination)],
                settled[static_cast<uint8_t>(destination - 1)],
                TETRIS_COLS);
        }
        memset(settled[0], 0, TETRIS_COLS);
        ++removed;
        // Recheck this row because a shifted row now occupies it.
    }
    return removed;
}

uint32_t line_clear_points(uint8_t lines)
{
    switch (lines) {
        case 1U: return 100UL;
        case 2U: return 300UL;
        case 3U: return 500UL;
        case 4U: return 800UL;
        default: return 0UL;
    }
}

void update_level_and_speed()
{
    uint16_t calculated_level = static_cast<uint16_t>(
        cleared_lines / TETRIS_LINES_PER_LEVEL + 1U);
    if (calculated_level > 255U) {
        calculated_level = 255U;
    }
    level = static_cast<uint8_t>(calculated_level);

    const uint32_t reduction =
        static_cast<uint32_t>(level - 1U) * TETRIS_LEVEL_STEP_MS;
    if (reduction + TETRIS_MINIMUM_FALL_MS >= TETRIS_INITIAL_FALL_MS) {
        fall_interval_ms = TETRIS_MINIMUM_FALL_MS;
    } else {
        fall_interval_ms = static_cast<uint16_t>(TETRIS_INITIAL_FALL_MS - reduction);
    }
}

bool lock_active_piece(uint32_t now_ms)
{
    const uint16_t mask = active_mask(active_piece, active_rotation);
    bool block_above_board = false;
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            if (!mask_has_block(mask, x, y)) {
                continue;
            }
            const int8_t board_x = static_cast<int8_t>(active_x + x);
            const int8_t board_y = static_cast<int8_t>(active_y + y);
            if (board_y < 0) {
                block_above_board = true;
                continue;
            }
            settled[board_y][board_x] = 1U;
            draw_board_cell(static_cast<uint8_t>(board_x), static_cast<uint8_t>(board_y));
        }
    }
    if (block_above_board) {
        return false;
    }

    const uint8_t removed = clear_complete_lines();
    if (removed > 0U) {
        cleared_lines = static_cast<uint16_t>(cleared_lines + removed);
        score += line_clear_points(removed) * level;
        update_level_and_speed();
        draw_board();
        draw_statistics();
    }
    return spawn_piece(now_ms);
}

bool move_active(int8_t delta_x, int8_t delta_y)
{
    const int8_t next_x = static_cast<int8_t>(active_x + delta_x);
    const int8_t next_y = static_cast<int8_t>(active_y + delta_y);
    if (!piece_fits(active_piece, active_rotation, next_x, next_y)) {
        return false;
    }
    erase_active_piece();
    active_x = next_x;
    active_y = next_y;
    draw_active_piece();
    return true;
}

void rotate_active()
{
    const uint8_t next_rotation = static_cast<uint8_t>((active_rotation + 1U) & 0x03U);
    const int8_t kicks[] = {0, -1, 1, -2, 2};
    for (uint8_t index = 0U; index < sizeof(kicks); ++index) {
        const int8_t candidate_x = static_cast<int8_t>(active_x + kicks[index]);
        if (!piece_fits(active_piece, next_rotation, candidate_x, active_y)) {
            continue;
        }
        erase_active_piece();
        active_x = candidate_x;
        active_rotation = next_rotation;
        draw_active_piece();
        return;
    }
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

void tetris_game_start(uint32_t now_ms)
{
    memset(settled, 0, sizeof(settled));
    bag_position = PIECE_COUNT;
    score = 0UL;
    cleared_lines = 0U;
    level = 1U;
    fall_interval_ms = TETRIS_INITIAL_FALL_MS;
    last_fall_ms = now_ms;

    active_piece = take_piece_from_bag();
    next_piece = take_piece_from_bag();
    active_rotation = 0U;
    active_x = 3;
    active_y = -1;
    draw_complete_game();
}

TetrisUpdateResult tetris_game_update(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::LEFT) {
        move_active(-1, 0);
    } else if (event == InputEvent::RIGHT) {
        move_active(1, 0);
    } else if (event == InputEvent::UP) {
        rotate_active();
    } else if (event == InputEvent::DOWN) {
        if (move_active(0, 1)) {
            ++score;
            last_fall_ms = now_ms;
            draw_stat("SCORE", score, 72);
        } else if (!lock_active_piece(now_ms)) {
            return TetrisUpdateResult::GAME_OVER;
        }
    } else if (event == InputEvent::SELECT) {
        erase_active_piece();
        uint16_t distance = 0U;
        while (piece_fits(
            active_piece, active_rotation, active_x,
            static_cast<int8_t>(active_y + 1))) {
            ++active_y;
            ++distance;
        }
        score += static_cast<uint32_t>(distance) * 2UL;
        if (!lock_active_piece(now_ms)) {
            return TetrisUpdateResult::GAME_OVER;
        }
        draw_stat("SCORE", score, 72);
        return TetrisUpdateResult::RUNNING;
    }

    if (static_cast<uint32_t>(now_ms - last_fall_ms) >= fall_interval_ms) {
        last_fall_ms = now_ms;
        if (!move_active(0, 1) && !lock_active_piece(now_ms)) {
            return TetrisUpdateResult::GAME_OVER;
        }
    }
    return TetrisUpdateResult::RUNNING;
}

void tetris_game_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void tetris_game_resume(uint32_t now_ms)
{
    last_fall_ms = now_ms;
    draw_complete_game();
}

void tetris_game_show_game_over()
{
    char score_text[17] = "SCORE ";
    format_u32(score, &score_text[6]);
    draw_overlay("GAME OVER", score_text, "PRESS RESTART  HOLD MENU");
}
