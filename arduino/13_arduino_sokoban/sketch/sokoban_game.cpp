#include "sokoban_game.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"
#include "level_generator.h"

namespace {

constexpr int8_t NO_CRATE = -1;

struct UndoEntry {
    uint8_t player_x;
    uint8_t player_y;
    uint8_t crate_index;
    uint8_t crate_x;
    uint8_t crate_y;
};

LevelLayout layout;
UndoEntry undo_entries[UNDO_DEPTH];
uint8_t undo_head = 0U;
uint8_t undo_count = 0U;

int16_t cell_pixel_x(uint8_t x)
{
    return static_cast<int16_t>(BOARD_X + x * BOARD_CELL_SIZE);
}

int16_t cell_pixel_y(uint8_t y)
{
    return static_cast<int16_t>(BOARD_Y + y * BOARD_CELL_SIZE);
}

int8_t crate_at(uint8_t x, uint8_t y)
{
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        if (layout.crate_x[crate] == x && layout.crate_y[crate] == y) {
            return static_cast<int8_t>(crate);
        }
    }
    return NO_CRATE;
}

bool is_wall(uint8_t x, uint8_t y)
{
    return layout.terrain[level_cell_index(x, y)] == TERRAIN_WALL;
}

bool is_target(uint8_t x, uint8_t y)
{
    return layout.terrain[level_cell_index(x, y)] == TERRAIN_TARGET;
}

bool cell_is_free(int8_t x, int8_t y)
{
    return x >= 0 && x < static_cast<int8_t>(LEVEL_COLS) &&
           y >= 0 && y < static_cast<int8_t>(LEVEL_ROWS) &&
           !is_wall(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) &&
           crate_at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) == NO_CRATE;
}

void copy_layout_to_session(SessionState *session)
{
    session->player_x = layout.player_x;
    session->player_y = layout.player_y;
    session->crate_count = layout.crate_count;
    memcpy(session->crate_x, layout.crate_x, sizeof(session->crate_x));
    memcpy(session->crate_y, layout.crate_y, sizeof(session->crate_y));
}

void copy_session_to_layout(const SessionState &session)
{
    layout.player_x = session.player_x;
    layout.player_y = session.player_y;
    layout.crate_count = session.crate_count;
    memcpy(layout.crate_x, session.crate_x, sizeof(layout.crate_x));
    memcpy(layout.crate_y, session.crate_y, sizeof(layout.crate_y));
}

void clear_undo_history()
{
    undo_head = 0U;
    undo_count = 0U;
}

void push_undo(const UndoEntry &entry)
{
    undo_entries[undo_head] = entry;
    undo_head = static_cast<uint8_t>((undo_head + 1U) % UNDO_DEPTH);
    if (undo_count < UNDO_DEPTH) {
        ++undo_count;
    }
}

bool pop_undo(UndoEntry *entry)
{
    if (entry == nullptr || undo_count == 0U) {
        return false;
    }
    undo_head = undo_head == 0U ?
        static_cast<uint8_t>(UNDO_DEPTH - 1U) :
        static_cast<uint8_t>(undo_head - 1U);
    *entry = undo_entries[undo_head];
    --undo_count;
    return true;
}

void draw_floor_cell(int16_t pixel_x, int16_t pixel_y, uint8_t x, uint8_t y)
{
    display_fill_rect(pixel_x, pixel_y, BOARD_CELL_SIZE, BOARD_CELL_SIZE,
                      COLOR_FLOOR);

    // Four deterministic one-pixel flecks reproduce the stippled gray floor
    // of early PC Sokoban without storing a bitmap or drawing random noise.
    const uint8_t phase = static_cast<uint8_t>((x * 5U + y * 7U) % 5U);
    for (uint8_t dot = 0U; dot < 4U; ++dot) {
        const int16_t dot_x = static_cast<int16_t>(
            pixel_x + 2 + (phase + dot * 6U) % 16U);
        const int16_t dot_y = static_cast<int16_t>(
            pixel_y + 2 + (phase * 3U + dot * 5U) % 16U);
        display_fill_rect(dot_x, dot_y, 1, 1, COLOR_FLOOR_DOT);
    }
}

void draw_brick_wall(int16_t x, int16_t y)
{
    display_fill_rect(x, y, BOARD_CELL_SIZE, BOARD_CELL_SIZE, COLOR_WALL_DARK);
    display_fill_rect(x + 1, y + 1, BOARD_CELL_SIZE - 2,
                      BOARD_CELL_SIZE - 2, COLOR_WALL_BRICK);

    // Three staggered courses form recognisable masonry instead of a generic
    // solid block. Highlights stay on the upper edge of each brick course.
    display_draw_line(x + 1, y + 6, x + 18, y + 6, COLOR_WALL_MORTAR);
    display_draw_line(x + 1, y + 13, x + 18, y + 13, COLOR_WALL_MORTAR);
    display_draw_line(x + 8, y + 1, x + 8, y + 5, COLOR_WALL_MORTAR);
    display_draw_line(x + 5, y + 7, x + 5, y + 12, COLOR_WALL_MORTAR);
    display_draw_line(x + 14, y + 7, x + 14, y + 12, COLOR_WALL_MORTAR);
    display_draw_line(x + 10, y + 14, x + 10, y + 18, COLOR_WALL_MORTAR);
    display_draw_line(x + 2, y + 2, x + 7, y + 2, COLOR_WALL_LIGHT);
    display_draw_line(x + 6, y + 9, x + 13, y + 9, COLOR_WALL_LIGHT);
    display_draw_line(x + 11, y + 16, x + 17, y + 16, COLOR_WALL_LIGHT);
}

void draw_target(int16_t x, int16_t y)
{
    // Layered rectangles approximate the large red circular goal markers in
    // the reference style without depending on a circle primitive.
    display_fill_rect(x + 7, y + 4, 6, 12, COLOR_TARGET_DARK);
    display_fill_rect(x + 4, y + 7, 12, 6, COLOR_TARGET_DARK);
    display_fill_rect(x + 7, y + 6, 6, 8, COLOR_TARGET);
    display_fill_rect(x + 6, y + 7, 8, 6, COLOR_TARGET);
}

void draw_crate(int16_t x, int16_t y, bool on_target)
{
    if (on_target) {
        display_draw_rect(x + 1, y + 1, 18, 18, COLOR_TARGET);
    }
    display_fill_rect(x + 4, y + 5, 16, 15, COLOR_PLAYER_DARK);
    display_fill_rect(x + 2, y + 2, 16, 16, COLOR_CRATE_DARK);
    display_fill_rect(x + 4, y + 4, 12, 12, COLOR_CRATE_WOOD);
    display_draw_rect(x + 3, y + 3, 14, 14, COLOR_CRATE_LIGHT);
    display_draw_line(x + 5, y + 5, x + 14, y + 14, COLOR_CRATE_DARK);
    display_draw_line(x + 14, y + 5, x + 5, y + 14, COLOR_CRATE_DARK);
    display_fill_rect(x + 4, y + 4, 2, 2, COLOR_CRATE_DARK);
    display_fill_rect(x + 14, y + 4, 2, 2, COLOR_CRATE_DARK);
    display_fill_rect(x + 4, y + 14, 2, 2, COLOR_CRATE_DARK);
    display_fill_rect(x + 14, y + 14, 2, 2, COLOR_CRATE_DARK);
    if (on_target) {
        display_fill_rect(x + 8, y + 8, 4, 4, COLOR_TARGET);
    }
}

void draw_player(int16_t x, int16_t y)
{
    // Compact front-facing worker sprite: blue trousers, green shirt, skin,
    // hair, and shoes remain legible at one 20-pixel tile.
    display_fill_rect(x + 5, y + 17, 11, 2, COLOR_PLAYER_DARK);
    display_fill_rect(x + 6, y + 13, 3, 5, COLOR_PLAYER_BLUE);
    display_fill_rect(x + 11, y + 13, 3, 5, COLOR_PLAYER_BLUE);
    display_fill_rect(x + 4, y + 17, 5, 2, COLOR_PLAYER_DARK);
    display_fill_rect(x + 12, y + 17, 5, 2, COLOR_PLAYER_DARK);
    display_fill_rect(x + 4, y + 8, 12, 6, COLOR_PLAYER_GREEN);
    display_fill_rect(x + 6, y + 8, 8, 2, COLOR_PLAYER_GREEN_LIGHT);
    display_fill_rect(x + 2, y + 9, 3, 5, COLOR_PLAYER_SKIN);
    display_fill_rect(x + 15, y + 9, 3, 5, COLOR_PLAYER_SKIN);
    display_fill_rect(x + 7, y + 2, 7, 6, COLOR_PLAYER_SKIN);
    display_fill_rect(x + 7, y + 1, 7, 3, COLOR_PLAYER_DARK);
    display_fill_rect(x + 8, y + 5, 2, 2, COLOR_PLAYER_DARK);
    display_fill_rect(x + 12, y + 5, 2, 2, COLOR_PLAYER_DARK);
}

void draw_cell(uint8_t x, uint8_t y)
{
    const int16_t pixel_x = cell_pixel_x(x);
    const int16_t pixel_y = cell_pixel_y(y);
    const uint8_t terrain = layout.terrain[level_cell_index(x, y)];
    if (terrain == TERRAIN_WALL) {
        draw_brick_wall(pixel_x, pixel_y);
        return;
    }
    draw_floor_cell(pixel_x, pixel_y, x, y);

    if (terrain == TERRAIN_TARGET) {
        draw_target(pixel_x, pixel_y);
    }

    const int8_t crate = crate_at(x, y);
    if (crate != NO_CRATE) {
        draw_crate(pixel_x, pixel_y, terrain == TERRAIN_TARGET);
    } else if (layout.player_x == x && layout.player_y == y) {
        draw_player(pixel_x, pixel_y);
    }
}

void append_text(char *destination, uint8_t capacity, const char *source)
{
    uint8_t position = static_cast<uint8_t>(strlen(destination));
    while (*source != '\0' && position + 1U < capacity) {
        destination[position++] = *source++;
    }
    destination[position] = '\0';
}

void append_u32(char *destination, uint8_t capacity, uint32_t value)
{
    char number[11];
    ultoa(value, number, 10);
    append_text(destination, capacity, number);
}

void append_level(char *destination, uint8_t capacity, uint16_t level_index)
{
    char number[4];
    const uint16_t number_value = static_cast<uint16_t>(level_index + 1U);
    number[0] = static_cast<char>('0' + (number_value / 100U) % 10U);
    number[1] = static_cast<char>('0' + (number_value / 10U) % 10U);
    number[2] = static_cast<char>('0' + number_value % 10U);
    number[3] = '\0';
    append_text(destination, capacity, number);
}

void draw_statistics(const SessionState &session)
{
    char text[52] = "L ";
    append_level(text, sizeof(text), session.level_index);
    append_text(text, sizeof(text), "  M ");
    append_u32(text, sizeof(text), session.moves);
    append_text(text, sizeof(text), "  P ");
    append_u32(text, sizeof(text), session.pushes);
    append_text(text, sizeof(text), "  S ");
    append_u32(text, sizeof(text), session.level_score);
    display_fill_rect(164, 28, 270, 20, COLOR_LCD);
    display_text(170, 32, text, 1U, COLOR_INK, COLOR_LCD);
}

void draw_board()
{
    display_fill_rect(
        BOARD_X, BOARD_Y,
        LEVEL_COLS * BOARD_CELL_SIZE, LEVEL_ROWS * BOARD_CELL_SIZE, COLOR_FLOOR);
    display_draw_rect(
        BOARD_X - 2, BOARD_Y - 2,
        LEVEL_COLS * BOARD_CELL_SIZE + 4,
        LEVEL_ROWS * BOARD_CELL_SIZE + 4, COLOR_WALL_DARK);
    for (uint8_t y = 0U; y < LEVEL_ROWS; ++y) {
        for (uint8_t x = 0U; x < LEVEL_COLS; ++x) {
            draw_cell(x, y);
        }
    }
}

void draw_overlay(
    const char *title,
    const char *line_two,
    const char *line_three,
    const char *line_four)
{
    display_fill_rect(96, 104, 288, 120, COLOR_LCD_SHADE);
    display_draw_rect(96, 104, 288, 120, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 118, title, 2U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(150, 156, line_two, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(150, 178, line_three, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(150, 200, line_four, 1U, COLOR_INK, COLOR_LCD_SHADE);
}

bool event_delta(InputEvent event, int8_t *delta_x, int8_t *delta_y)
{
    if (delta_x == nullptr || delta_y == nullptr) {
        return false;
    }
    *delta_x = 0;
    *delta_y = 0;
    switch (event) {
        case InputEvent::UP: *delta_y = -1; return true;
        case InputEvent::DOWN: *delta_y = 1; return true;
        case InputEvent::LEFT: *delta_x = -1; return true;
        case InputEvent::RIGHT: *delta_x = 1; return true;
        default: return false;
    }
}

void subtract_score(SessionState *session, uint8_t amount)
{
    if (session->level_score > amount) {
        session->level_score -= amount;
    } else {
        session->level_score = 0UL;
    }
}

SokobanActionResult undo_move(SessionState *session)
{
    UndoEntry entry;
    if (!pop_undo(&entry)) {
        return SokobanActionResult::NO_CHANGE;
    }
    const uint8_t current_player_x = layout.player_x;
    const uint8_t current_player_y = layout.player_y;
    layout.player_x = entry.player_x;
    layout.player_y = entry.player_y;
    if (session->moves > 0U) {
        --session->moves;
    }
    session->level_score += 1UL;

    draw_cell(current_player_x, current_player_y);
    if (entry.crate_index < layout.crate_count) {
        const uint8_t current_crate_x = layout.crate_x[entry.crate_index];
        const uint8_t current_crate_y = layout.crate_y[entry.crate_index];
        layout.crate_x[entry.crate_index] = entry.crate_x;
        layout.crate_y[entry.crate_index] = entry.crate_y;
        if (session->pushes > 0U) {
            --session->pushes;
        }
        session->level_score += 4UL;
        draw_cell(current_crate_x, current_crate_y);
        draw_cell(entry.crate_x, entry.crate_y);
    }
    if (session->level_score > layout.starting_score) {
        session->level_score = layout.starting_score;
    }
    draw_cell(layout.player_x, layout.player_y);
    copy_layout_to_session(session);
    draw_statistics(*session);
    return SokobanActionResult::CHANGED;
}

}  // namespace

bool sokoban_game_start(
    uint16_t level_index, GameMode mode, SessionState *session)
{
    if (session == nullptr || !level_generate(level_index, &layout)) {
        return false;
    }
    session->active = true;
    session->mode = mode;
    session->level_index = level_index;
    session->moves = 0U;
    session->pushes = 0U;
    session->level_score = layout.starting_score;
    copy_layout_to_session(session);
    clear_undo_history();
    sokoban_game_redraw(*session);
    return true;
}

bool sokoban_game_restore(SessionState *session)
{
    if (session == nullptr || !session->active ||
        !level_generate(session->level_index, &layout) ||
        session->crate_count != layout.crate_count) {
        return false;
    }
    copy_session_to_layout(*session);
    if (!level_entities_are_valid(layout)) {
        return false;
    }
    if (session->level_score > layout.starting_score) {
        session->level_score = layout.starting_score;
    }
    clear_undo_history();
    sokoban_game_redraw(*session);
    return true;
}

SokobanActionResult sokoban_game_handle_input(
    InputEvent event, SessionState *session)
{
    if (session == nullptr) {
        return SokobanActionResult::NO_CHANGE;
    }
    if (event == InputEvent::UNDO) {
        return undo_move(session);
    }

    int8_t delta_x = 0;
    int8_t delta_y = 0;
    if (!event_delta(event, &delta_x, &delta_y)) {
        return SokobanActionResult::NO_CHANGE;
    }
    const int8_t next_x = static_cast<int8_t>(layout.player_x + delta_x);
    const int8_t next_y = static_cast<int8_t>(layout.player_y + delta_y);
    if (!cell_is_free(next_x, next_y)) {
        if (next_x < 0 || next_x >= static_cast<int8_t>(LEVEL_COLS) ||
            next_y < 0 || next_y >= static_cast<int8_t>(LEVEL_ROWS) ||
            is_wall(static_cast<uint8_t>(next_x), static_cast<uint8_t>(next_y))) {
            return SokobanActionResult::NO_CHANGE;
        }
    }

    const int8_t crate = crate_at(
        static_cast<uint8_t>(next_x), static_cast<uint8_t>(next_y));
    int8_t beyond_x = next_x;
    int8_t beyond_y = next_y;
    if (crate != NO_CRATE) {
        beyond_x = static_cast<int8_t>(next_x + delta_x);
        beyond_y = static_cast<int8_t>(next_y + delta_y);
        if (!cell_is_free(beyond_x, beyond_y)) {
            return SokobanActionResult::NO_CHANGE;
        }
    }

    UndoEntry undo;
    undo.player_x = layout.player_x;
    undo.player_y = layout.player_y;
    undo.crate_index = crate == NO_CRATE ?
        static_cast<uint8_t>(0xFFU) : static_cast<uint8_t>(crate);
    undo.crate_x = crate == NO_CRATE ?
        static_cast<uint8_t>(0U) : layout.crate_x[static_cast<uint8_t>(crate)];
    undo.crate_y = crate == NO_CRATE ?
        static_cast<uint8_t>(0U) : layout.crate_y[static_cast<uint8_t>(crate)];
    push_undo(undo);

    const uint8_t previous_player_x = layout.player_x;
    const uint8_t previous_player_y = layout.player_y;
    layout.player_x = static_cast<uint8_t>(next_x);
    layout.player_y = static_cast<uint8_t>(next_y);
    ++session->moves;
    subtract_score(session, 1U);

    draw_cell(previous_player_x, previous_player_y);
    if (crate != NO_CRATE) {
        layout.crate_x[static_cast<uint8_t>(crate)] = static_cast<uint8_t>(beyond_x);
        layout.crate_y[static_cast<uint8_t>(crate)] = static_cast<uint8_t>(beyond_y);
        ++session->pushes;
        subtract_score(session, 4U);
        draw_cell(static_cast<uint8_t>(beyond_x), static_cast<uint8_t>(beyond_y));
    }
    draw_cell(layout.player_x, layout.player_y);
    copy_layout_to_session(session);
    draw_statistics(*session);
    return sokoban_game_is_solved() ?
        SokobanActionResult::WON : SokobanActionResult::CHANGED;
}

bool sokoban_game_is_solved()
{
    for (uint8_t crate = 0U; crate < layout.crate_count; ++crate) {
        if (!is_target(layout.crate_x[crate], layout.crate_y[crate])) {
            return false;
        }
    }
    return true;
}

void sokoban_game_redraw(const SessionState &session)
{
    display_draw_lcd_panel();
    display_text(46, 30, "SOKOBAN", 2U, COLOR_INK, COLOR_LCD);
    draw_statistics(session);
    draw_board();
}

void sokoban_game_show_paused()
{
    draw_overlay("PAUSED", "P/BUTTON - RESUME", "R - RESTART", "HOLD/Q - MENU");
}

void sokoban_game_show_complete(const SessionState &session)
{
    char score_text[22] = "SCORE ";
    ultoa(session.level_score, &score_text[6], 10);
    draw_overlay("LEVEL CLEAR", score_text, "ENTER/BUTTON - NEXT",
                 "R REPLAY  HOLD/Q MENU");
}
