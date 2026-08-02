#include "sokoban_game.h"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

enum Tile : uint8_t {
    VOID_TILE,
    FLOOR,
    WALL,
    TARGET,
    CRATE,
    CRATE_ON_TARGET,
    PLAYER,
    PLAYER_ON_TARGET,
};

struct LevelDefinition {
    PGM_P map;
    uint8_t width;
    uint8_t height;
};

// Rows are concatenated without delimiters. Explicit dimensions permit
// different map sizes while compile-time checks catch malformed level data.
const char level_1[] PROGMEM =
    "############"
    "#          #"
    "#  . . .   #"
    "#          #"
    "#  $ $ $   #"
    "#          #"
    "#    @     #"
    "#          #"
    "############";

const char level_2[] PROGMEM =
    "################"
    "#              #"
    "#  .  .  .     #"
    "#              #"
    "#    ##        #"
    "#  $  $  $     #"
    "#              #"
    "#       @      #"
    "#              #"
    "################";

const char level_3[] PROGMEM =
    "##################"
    "#                #"
    "#  .   .   .  .  #"
    "#                #"
    "#    #   #       #"
    "#                #"
    "#  $   $   $  $  #"
    "#                #"
    "#        @       #"
    "#                #"
    "##################";

const char level_4[] PROGMEM =
    "####################"
    "#                  #"
    "#  .  .  .  .  .   #"
    "#                  #"
    "#    #     #       #"
    "#                  #"
    "#  $  $  $  $  $   #"
    "#                  #"
    "#        @         #"
    "#                  #"
    "#                  #"
    "####################";

const char level_5[] PROGMEM =
    "######################"
    "#                    #"
    "#  .   .   .   .  .  #"
    "#                    #"
    "#    ##      ##      #"
    "#                    #"
    "#                    #"
    "#  $   $   $   $  $  #"
    "#                    #"
    "#          @         #"
    "#                    #"
    "#                    #"
    "######################";

static_assert(sizeof(level_1) - 1U == 12U * 9U, "Sokoban level 1 size");
static_assert(sizeof(level_2) - 1U == 16U * 10U, "Sokoban level 2 size");
static_assert(sizeof(level_3) - 1U == 18U * 11U, "Sokoban level 3 size");
static_assert(sizeof(level_4) - 1U == 20U * 12U, "Sokoban level 4 size");
static_assert(sizeof(level_5) - 1U == 22U * 13U, "Sokoban level 5 size");

const LevelDefinition levels[] PROGMEM = {
    {level_1, 12U, 9U},
    {level_2, 16U, 10U},
    {level_3, 18U, 11U},
    {level_4, 20U, 12U},
    {level_5, 22U, 13U},
};

constexpr uint8_t LEVEL_COUNT = sizeof(levels) / sizeof(levels[0]);

uint8_t board[SOKOBAN_MAX_ROWS][SOKOBAN_MAX_COLS];
uint8_t selected_level = 0U;
uint8_t level_width = 0U;
uint8_t level_height = 0U;
uint8_t player_x = 0U;
uint8_t player_y = 0U;
uint8_t target_count = 0U;
uint16_t moves = 0U;
uint16_t pushes = 0U;
uint16_t score = 0U;
int16_t board_x = SOKOBAN_PLAY_LEFT;
int16_t board_y = SOKOBAN_PLAY_TOP;

PGM_P level_map(uint8_t index)
{
    return reinterpret_cast<PGM_P>(pgm_read_ptr(&levels[index].map));
}

uint8_t level_map_width(uint8_t index)
{
    return pgm_read_byte(&levels[index].width);
}

uint8_t level_map_height(uint8_t index)
{
    return pgm_read_byte(&levels[index].height);
}

bool is_target_tile(uint8_t tile)
{
    return tile == TARGET || tile == CRATE_ON_TARGET ||
           tile == PLAYER_ON_TARGET;
}

bool is_crate(uint8_t tile)
{
    return tile == CRATE || tile == CRATE_ON_TARGET;
}

bool is_blocked(uint8_t tile)
{
    return tile == VOID_TILE || tile == WALL || is_crate(tile);
}

int16_t cell_x(uint8_t x)
{
    return static_cast<int16_t>(
        board_x + static_cast<int16_t>(x) * SOKOBAN_TILE_SIZE);
}

int16_t cell_y(uint8_t y)
{
    return static_cast<int16_t>(
        board_y + static_cast<int16_t>(y) * SOKOBAN_TILE_SIZE);
}

void draw_target(int16_t x, int16_t y)
{
    display_draw_rect(x + 5, y + 5, 8, 8, COLOR_INK_LIGHT);
    display_fill_rect(x + 8, y + 8, 2, 2, COLOR_INK_LIGHT);
}

void draw_cell(uint8_t x, uint8_t y)
{
    if (x >= level_width || y >= level_height) {
        return;
    }
    const uint8_t tile = board[y][x];
    const int16_t pixel_x = cell_x(x);
    const int16_t pixel_y = cell_y(y);
    display_fill_rect(pixel_x, pixel_y, SOKOBAN_TILE_SIZE,
                      SOKOBAN_TILE_SIZE, COLOR_LCD);

    if (tile == VOID_TILE) {
        return;
    }
    if (tile == WALL) {
        // Heavy filled masonry has no diagonal crate motif.
        display_fill_rect(pixel_x + 1, pixel_y + 1, 16, 16, COLOR_INK);
        display_draw_rect(pixel_x + 2, pixel_y + 2, 14, 14, COLOR_LCD_SHADE);
        display_draw_line(pixel_x + 2, pixel_y + 8,
                          pixel_x + 15, pixel_y + 8, COLOR_LCD_SHADE);
        display_draw_line(pixel_x + 8, pixel_y + 2,
                          pixel_x + 8, pixel_y + 8, COLOR_LCD_SHADE);
        display_draw_line(pixel_x + 5, pixel_y + 9,
                          pixel_x + 5, pixel_y + 15, COLOR_LCD_SHADE);
        return;
    }

    if (is_target_tile(tile)) {
        draw_target(pixel_x, pixel_y);
    }
    if (is_crate(tile)) {
        // Thin framed X makes a movable crate distinct from a solid wall.
        display_fill_rect(pixel_x + 3, pixel_y + 3, 12, 12, COLOR_LCD_SHADE);
        display_draw_rect(pixel_x + 2, pixel_y + 2, 14, 14, COLOR_INK);
        display_draw_line(pixel_x + 4, pixel_y + 4,
                          pixel_x + 13, pixel_y + 13, COLOR_INK);
        display_draw_line(pixel_x + 13, pixel_y + 4,
                          pixel_x + 4, pixel_y + 13, COLOR_INK);
        if (tile == CRATE_ON_TARGET) {
            display_draw_rect(pixel_x + 4, pixel_y + 4, 10, 10, COLOR_INK_LIGHT);
            display_fill_rect(pixel_x + 8, pixel_y + 8, 2, 2, COLOR_LCD);
        }
    } else if (tile == PLAYER || tile == PLAYER_ON_TARGET) {
        display_fill_rect(pixel_x + 4, pixel_y + 2, 10, 14, COLOR_INK);
        display_fill_rect(pixel_x + 6, pixel_y + 6, 2, 2, COLOR_LCD);
        display_fill_rect(pixel_x + 10, pixel_y + 6, 2, 2, COLOR_LCD);
        display_draw_line(pixel_x + 6, pixel_y + 12,
                          pixel_x + 11, pixel_y + 12, COLOR_LCD);
    }
}

void draw_number_field(int16_t x, const char *prefix, uint16_t value,
                       int16_t field_width)
{
    char text[12];
    strncpy(text, prefix, sizeof(text) - 1U);
    text[sizeof(text) - 1U] = '\0';
    const uint8_t length = static_cast<uint8_t>(strlen(text));
    utoa(value, &text[length], 10);
    display_fill_rect(x, 31, field_width, 12, COLOR_LCD);
    display_text(x, 32, text, 1U, COLOR_INK, COLOR_LCD);
}

void draw_all_statistics()
{
    draw_number_field(178, "L:", static_cast<uint16_t>(selected_level + 1U), 46);
    draw_number_field(228, "M:", moves, 60);
    draw_number_field(292, "P:", pushes, 60);
    draw_number_field(356, "S:", score, 78);
}

void draw_move_statistics(bool pushed)
{
    draw_number_field(228, "M:", moves, 60);
    if (pushed) {
        draw_number_field(292, "P:", pushes, 60);
    }
    draw_number_field(356, "S:", score, 78);
}

void draw_board()
{
    display_fill_rect(SOKOBAN_PLAY_LEFT, SOKOBAN_PLAY_TOP,
                      SOKOBAN_PLAY_WIDTH, SOKOBAN_PLAY_HEIGHT, COLOR_LCD);
    const int16_t width = static_cast<int16_t>(level_width) * SOKOBAN_TILE_SIZE;
    const int16_t height = static_cast<int16_t>(level_height) * SOKOBAN_TILE_SIZE;
    display_draw_rect(board_x - 2, board_y - 2, width + 4, height + 4,
                      COLOR_INK_LIGHT);
    for (uint8_t y = 0U; y < level_height; ++y) {
        for (uint8_t x = 0U; x < level_width; ++x) {
            draw_cell(x, y);
        }
    }
}

void draw_complete_game()
{
    display_draw_lcd_panel();
    display_text(44, 29, "SOKOBAN", 2U, COLOR_INK, COLOR_LCD);
    draw_all_statistics();
    draw_board();
}

void draw_level_selector()
{
    display_draw_lcd_panel();
    display_text(145, 34, "SOKOBAN LEVEL", 2U, COLOR_INK, COLOR_LCD);
    display_draw_rect(116, 82, 248, 124, COLOR_INK_LIGHT);

    char level_text[18] = "LEVEL ";
    utoa(static_cast<uint16_t>(selected_level + 1U), &level_text[6], 10);
    uint8_t length = static_cast<uint8_t>(strlen(level_text));
    level_text[length++] = ' ';
    level_text[length++] = '/';
    level_text[length++] = ' ';
    utoa(LEVEL_COUNT, &level_text[length], 10);
    display_fill_rect(158, 105, 180, 30, COLOR_LCD);
    display_text(158, 106, level_text, 2U, COLOR_INK, COLOR_LCD);

    char size_text[14];
    utoa(level_map_width(selected_level), size_text, 10);
    length = static_cast<uint8_t>(strlen(size_text));
    size_text[length++] = ' ';
    size_text[length++] = 'x';
    size_text[length++] = ' ';
    utoa(level_map_height(selected_level), &size_text[length], 10);
    display_fill_rect(198, 154, 100, 12, COLOR_LCD);
    display_text(198, 154, size_text, 1U, COLOR_INK_LIGHT, COLOR_LCD);
    display_text(146, 181, "LEFT/RIGHT - CHOOSE", 1U, COLOR_INK, COLOR_LCD);
    display_text(162, 230, "PRESS - PLAY", 1U, COLOR_INK, COLOR_LCD);
    display_text(162, 250, "HOLD - MENU", 1U, COLOR_INK, COLOR_LCD);
}

bool load_selected_level()
{
    level_width = level_map_width(selected_level);
    level_height = level_map_height(selected_level);
    if (level_width == 0U || level_width > SOKOBAN_MAX_COLS ||
        level_height == 0U || level_height > SOKOBAN_MAX_ROWS) {
        Serial.println(F("Sokoban: invalid level dimensions"));
        return false;
    }

    memset(board, VOID_TILE, sizeof(board));
    player_x = 0U;
    player_y = 0U;
    moves = 0U;
    pushes = 0U;
    target_count = 0U;
    uint8_t crate_count = 0U;
    uint8_t player_count = 0U;
    PGM_P map = level_map(selected_level);
    for (uint8_t y = 0U; y < level_height; ++y) {
        for (uint8_t x = 0U; x < level_width; ++x) {
            const char symbol = static_cast<char>(pgm_read_byte(
                map + static_cast<uint16_t>(y) * level_width + x));
            uint8_t tile = FLOOR;
            if (symbol == '#') {
                tile = WALL;
            } else if (symbol == '.') {
                tile = TARGET;
                ++target_count;
            } else if (symbol == '$') {
                tile = CRATE;
                ++crate_count;
            } else if (symbol == '*') {
                tile = CRATE_ON_TARGET;
                ++crate_count;
                ++target_count;
            } else if (symbol == '@') {
                tile = PLAYER;
                player_x = x;
                player_y = y;
                ++player_count;
            } else if (symbol == '+') {
                tile = PLAYER_ON_TARGET;
                player_x = x;
                player_y = y;
                ++player_count;
                ++target_count;
            } else if (symbol != ' ') {
                tile = VOID_TILE;
            }
            board[y][x] = tile;
        }
    }
    if (player_count != 1U || crate_count == 0U || crate_count != target_count) {
        Serial.println(F("Sokoban: invalid player/crate/target count"));
        return false;
    }

    const int16_t pixel_width =
        static_cast<int16_t>(level_width) * SOKOBAN_TILE_SIZE;
    const int16_t pixel_height =
        static_cast<int16_t>(level_height) * SOKOBAN_TILE_SIZE;
    board_x = static_cast<int16_t>(
        SOKOBAN_PLAY_LEFT + (SOKOBAN_PLAY_WIDTH - pixel_width) / 2);
    board_y = static_cast<int16_t>(
        SOKOBAN_PLAY_TOP + (SOKOBAN_PLAY_HEIGHT - pixel_height) / 2);
    score = static_cast<uint16_t>(3000U + selected_level * 250U);

    Serial.print(F("Sokoban: level "));
    Serial.print(static_cast<uint16_t>(selected_level + 1U));
    Serial.print(F(" dimensions "));
    Serial.print(level_width);
    Serial.print('x');
    Serial.println(level_height);
    return true;
}

bool all_crates_are_on_targets()
{
    uint8_t completed = 0U;
    for (uint8_t y = 0U; y < level_height; ++y) {
        for (uint8_t x = 0U; x < level_width; ++x) {
            if (board[y][x] == CRATE_ON_TARGET) {
                ++completed;
            }
        }
    }
    return completed == target_count;
}

bool event_delta(InputEvent event, int8_t &delta_x, int8_t &delta_y)
{
    delta_x = 0;
    delta_y = 0;
    switch (event) {
        case InputEvent::UP: delta_y = -1; return true;
        case InputEvent::DOWN: delta_y = 1; return true;
        case InputEvent::LEFT: delta_x = -1; return true;
        case InputEvent::RIGHT: delta_x = 1; return true;
        default: return false;
    }
}

void draw_overlay(const char *title, const char *line_two, const char *line_three)
{
    display_fill_rect(106, 113, 268, 96, COLOR_LCD_SHADE);
    display_draw_rect(106, 113, 268, 96, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 128, title, 2U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(142, 165, line_two, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(142, 185, line_three, 1U, COLOR_INK, COLOR_LCD_SHADE);
}

}  // namespace

void sokoban_level_select_enter()
{
    draw_level_selector();
}

bool sokoban_level_select_update(InputEvent event)
{
    if (event == InputEvent::LEFT || event == InputEvent::UP) {
        selected_level = selected_level == 0U ?
            static_cast<uint8_t>(LEVEL_COUNT - 1U) :
            static_cast<uint8_t>(selected_level - 1U);
        draw_level_selector();
    } else if (event == InputEvent::RIGHT || event == InputEvent::DOWN) {
        selected_level = static_cast<uint8_t>((selected_level + 1U) % LEVEL_COUNT);
        draw_level_selector();
    }
    return event == InputEvent::SELECT;
}

void sokoban_game_start_selected()
{
    if (!load_selected_level()) {
        selected_level = 0U;
        load_selected_level();
    }
    draw_complete_game();
}

void sokoban_game_start_next()
{
    selected_level = static_cast<uint8_t>((selected_level + 1U) % LEVEL_COUNT);
    sokoban_game_start_selected();
}

SokobanUpdateResult sokoban_game_update(InputEvent event)
{
    int8_t delta_x = 0;
    int8_t delta_y = 0;
    if (!event_delta(event, delta_x, delta_y)) {
        return SokobanUpdateResult::RUNNING;
    }

    const int8_t next_x = static_cast<int8_t>(player_x + delta_x);
    const int8_t next_y = static_cast<int8_t>(player_y + delta_y);
    if (next_x < 0 || next_x >= static_cast<int8_t>(level_width) ||
        next_y < 0 || next_y >= static_cast<int8_t>(level_height)) {
        return SokobanUpdateResult::RUNNING;
    }

    uint8_t destination = board[static_cast<uint8_t>(next_y)]
                               [static_cast<uint8_t>(next_x)];
    if (is_blocked(destination) && !is_crate(destination)) {
        return SokobanUpdateResult::RUNNING;
    }

    bool pushed = false;
    int8_t beyond_x = next_x;
    int8_t beyond_y = next_y;
    if (is_crate(destination)) {
        beyond_x = static_cast<int8_t>(next_x + delta_x);
        beyond_y = static_cast<int8_t>(next_y + delta_y);
        if (beyond_x < 0 || beyond_x >= static_cast<int8_t>(level_width) ||
            beyond_y < 0 || beyond_y >= static_cast<int8_t>(level_height)) {
            return SokobanUpdateResult::RUNNING;
        }
        const uint8_t beyond = board[static_cast<uint8_t>(beyond_y)]
                                    [static_cast<uint8_t>(beyond_x)];
        if (is_blocked(beyond)) {
            return SokobanUpdateResult::RUNNING;
        }
        board[static_cast<uint8_t>(beyond_y)][static_cast<uint8_t>(beyond_x)] =
            is_target_tile(beyond) ? CRATE_ON_TARGET : CRATE;
        board[static_cast<uint8_t>(next_y)][static_cast<uint8_t>(next_x)] =
            is_target_tile(destination) ? TARGET : FLOOR;
        destination = board[static_cast<uint8_t>(next_y)]
                           [static_cast<uint8_t>(next_x)];
        ++pushes;
        pushed = true;
    }

    board[player_y][player_x] =
        board[player_y][player_x] == PLAYER_ON_TARGET ? TARGET : FLOOR;
    board[static_cast<uint8_t>(next_y)][static_cast<uint8_t>(next_x)] =
        is_target_tile(destination) ? PLAYER_ON_TARGET : PLAYER;

    const uint8_t previous_x = player_x;
    const uint8_t previous_y = player_y;
    player_x = static_cast<uint8_t>(next_x);
    player_y = static_cast<uint8_t>(next_y);
    ++moves;
    const uint16_t penalty = pushed ? 5U : 1U;
    score = score > penalty ? static_cast<uint16_t>(score - penalty) : 0U;

    // Turn-based dirty rendering: only the vacated player cell, the new
    // player cell, an optional pushed-crate destination, and changed counters.
    draw_cell(previous_x, previous_y);
    draw_cell(player_x, player_y);
    if (pushed) {
        draw_cell(static_cast<uint8_t>(beyond_x), static_cast<uint8_t>(beyond_y));
    }
    draw_move_statistics(pushed);

    return all_crates_are_on_targets() ?
        SokobanUpdateResult::WON : SokobanUpdateResult::RUNNING;
}

void sokoban_game_show_paused()
{
    draw_overlay("PAUSED", "PRESS/UP - RESUME", "DOWN RESET  HOLD MENU");
}

void sokoban_game_resume()
{
    draw_complete_game();
}

void sokoban_game_show_won()
{
    char moves_text[18] = "MOVES ";
    utoa(moves, &moves_text[6], 10);
    draw_overlay("LEVEL CLEAR", moves_text, "PRESS NEXT  HOLD MENU");
}
