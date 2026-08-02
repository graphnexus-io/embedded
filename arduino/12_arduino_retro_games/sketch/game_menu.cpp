#include "game_menu.h"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

struct MenuEntry {
    PGM_P name;
    GameId game;
};

const char snake_name[] PROGMEM = "SNAKE";
const char tetris_name[] PROGMEM = "TETRIS";
const char breakout_name[] PROGMEM = "BREAKOUT";
const char racing_name[] PROGMEM = "RETRO RACING";
const char game_2048_name[] PROGMEM = "2048";
const char sokoban_name[] PROGMEM = "SOKOBAN";
const char lander_name[] PROGMEM = "LUNAR LANDER";
const MenuEntry menu_entries[] PROGMEM = {
    {snake_name, GameId::SNAKE},
    {tetris_name, GameId::TETRIS},
    {breakout_name, GameId::BREAKOUT},
    {racing_name, GameId::RACING},
    {game_2048_name, GameId::GAME_2048},
    {sokoban_name, GameId::SOKOBAN},
    {lander_name, GameId::LUNAR_LANDER},
};

constexpr uint8_t MENU_ENTRY_COUNT =
    static_cast<uint8_t>(sizeof(menu_entries) / sizeof(menu_entries[0]));
constexpr uint8_t MENU_VISIBLE_COUNT = 5U;
constexpr int16_t MENU_FIRST_Y = 78;
constexpr int16_t MENU_ROW_HEIGHT = 31;

uint8_t selected_entry = 0U;
uint8_t first_visible_entry = 0U;

PGM_P entry_name(uint8_t index)
{
    return reinterpret_cast<PGM_P>(pgm_read_ptr(&menu_entries[index].name));
}

GameId entry_game(uint8_t index)
{
    return static_cast<GameId>(pgm_read_byte(&menu_entries[index].game));
}

void draw_entry(uint8_t index, uint8_t visible_row)
{
    char name[17];
    strncpy_P(name, entry_name(index), sizeof(name) - 1U);
    name[sizeof(name) - 1U] = '\0';

    char line[24];
    uint8_t position = 0U;
    line[position++] = index == selected_entry ? '>' : ' ';
    line[position++] = ' ';
    line[position++] = static_cast<char>('0' + ((index + 1U) / 10U));
    line[position++] = static_cast<char>('0' + ((index + 1U) % 10U));
    line[position++] = ' ';
    for (uint8_t source = 0U;
         name[source] != '\0' && position < sizeof(line) - 1U;
         ++source) {
        line[position++] = name[source];
    }
    line[position] = '\0';

    const int16_t y = static_cast<int16_t>(
        MENU_FIRST_Y + visible_row * MENU_ROW_HEIGHT);
    display_fill_rect(126, y - 4, 228, 24, COLOR_LCD);
    display_text(136, y, line, 2U, COLOR_INK, COLOR_LCD);
}

void draw_visible_entries()
{
    display_fill_rect(124, 69, 232, 162, COLOR_LCD);
    for (uint8_t row = 0U; row < MENU_VISIBLE_COUNT; ++row) {
        const uint8_t index = static_cast<uint8_t>(first_visible_entry + row);
        if (index < MENU_ENTRY_COUNT) {
            draw_entry(index, row);
        }
    }

    char page[8] = "1-1/1";
    page[0] = static_cast<char>('1' + first_visible_entry);
    uint8_t last = static_cast<uint8_t>(
        first_visible_entry + MENU_VISIBLE_COUNT);
    if (last > MENU_ENTRY_COUNT) {
        last = MENU_ENTRY_COUNT;
    }
    page[2] = static_cast<char>('0' + last);
    page[4] = static_cast<char>('0' + MENU_ENTRY_COUNT);
    display_fill_rect(372, 214, 48, 12, COLOR_LCD);
    display_text(376, 214, page, 1U, COLOR_INK_LIGHT, COLOR_LCD);
}

void draw_menu()
{
    display_draw_lcd_panel();
    display_text(174, 34, "RETRO GAMES", 2U, COLOR_INK, COLOR_LCD);
    display_draw_rect(118, 64, 244, 172, COLOR_INK_LIGHT);
    draw_visible_entries();
    display_text(174, 250, "PRESS/ENTER - START", 1U, COLOR_INK, COLOR_LCD);
    display_text(162, 270, "STICK OR W/S - SELECT", 1U, COLOR_INK, COLOR_LCD);
}

}  // namespace

void game_menu_enter()
{
    selected_entry = 0U;
    first_visible_entry = 0U;
    draw_menu();
}

GameId game_menu_handle_input(InputEvent event)
{
    if (event == InputEvent::UP) {
        selected_entry = selected_entry == 0U ?
            static_cast<uint8_t>(MENU_ENTRY_COUNT - 1U) :
            static_cast<uint8_t>(selected_entry - 1U);
    } else if (event == InputEvent::DOWN) {
        selected_entry = static_cast<uint8_t>((selected_entry + 1U) % MENU_ENTRY_COUNT);
    } else if (event == InputEvent::SELECT) {
        return entry_game(selected_entry);
    }

    if (event == InputEvent::UP || event == InputEvent::DOWN) {
        if (selected_entry < first_visible_entry) {
            first_visible_entry = selected_entry;
        } else if (selected_entry >= first_visible_entry + MENU_VISIBLE_COUNT) {
            first_visible_entry = static_cast<uint8_t>(
                selected_entry - MENU_VISIBLE_COUNT + 1U);
        }
        draw_visible_entries();
    }
    // MENU is the top-level screen in this version, so BACK has no parent.
    return GameId::NONE;
}
