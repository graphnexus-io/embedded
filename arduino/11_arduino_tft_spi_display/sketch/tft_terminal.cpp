#include "tft_terminal.h"

#include <LCDWIKI_GUI.h>
#include <LCDWIKI_SPI.h>
#include <SPI.h>

#include "config.h"

namespace {

// Restrained monochrome phosphor palette. There are deliberately no separate
// header, footer, widget, or status colors.
constexpr uint16_t COLOR_BACKGROUND = 0x0000;
constexpr uint16_t COLOR_TEXT = 0xBFE0;
constexpr uint16_t COLOR_FRAME = 0x4BE0;

constexpr int16_t SCREEN_WIDTH = 480;
constexpr int16_t SCREEN_HEIGHT = 320;
constexpr int16_t OUTPUT_LEFT = 8;
constexpr int16_t OUTPUT_TOP = 8;
constexpr int16_t LINE_HEIGHT = 10;
constexpr int16_t CHARACTER_WIDTH = 6;
constexpr int16_t CURSOR_WIDTH = 5;
constexpr int16_t CURSOR_Y_OFFSET = 8;

LCDWIKI_SPI display(ST7796S, LCD_CS_PIN, LCD_RS_PIN, LCD_RST_PIN, LCD_LED_PIN);
const uint8_t benchmark_text[] = "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Lines are stored as a ring. At size 1, 76 characters fit comfortably.
// Longer logical lines wrap; the oldest line is overwritten on scrolling.
char lines[TERMINAL_VISIBLE_LINES][TERMINAL_LINE_LENGTH + 1U];
uint8_t first_line = 0U;
uint8_t line_count = 1U;
uint8_t current_length = 0U;
uint8_t update_depth = 0U;
uint32_t dirty_lines = 0UL;
uint32_t erase_remainder_lines = 0UL;
uint8_t dirty_from[TERMINAL_VISIBLE_LINES];
bool full_redraw_needed = false;
bool cursor_enabled = false;
bool cursor_drawn = false;
bool cursor_is_absolute = false;
uint8_t cursor_characters_after = 0U;
uint8_t absolute_cursor_row = 0U;
uint8_t absolute_cursor_column = 0U;
int16_t cursor_x = OUTPUT_LEFT;
int16_t cursor_y = OUTPUT_TOP + CURSOR_Y_OFFSET;

void prepare_display_bus()
{
    // LCDWIKI_SPI controls LCD_CS around its own transfers but does not use
    // SPI transactions. Keep the SD reader idle and restore the settings used
    // by LCDWIKI after an SD-library transaction changed them.
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
}

uint8_t physical_index(uint8_t logical_index)
{
    return static_cast<uint8_t>((first_line + logical_index) % TERMINAL_VISIBLE_LINES);
}

void erase_cursor()
{
    if (!cursor_drawn) {
        return;
    }
    display.Set_Draw_color(COLOR_BACKGROUND);
    display.Fill_Rectangle(
        cursor_x, cursor_y, cursor_x + CURSOR_WIDTH - 1, cursor_y);
    cursor_drawn = false;
}

void draw_cursor()
{
    if (!cursor_enabled || line_count == 0U) {
        return;
    }

    uint8_t logical = cursor_is_absolute ? absolute_cursor_row :
        static_cast<uint8_t>(line_count - 1U);
    uint8_t column = cursor_is_absolute ? absolute_cursor_column : current_length;
    if (logical >= TERMINAL_VISIBLE_LINES) {
        logical = static_cast<uint8_t>(TERMINAL_VISIBLE_LINES - 1U);
    }
    if (column > TERMINAL_LINE_LENGTH) {
        column = TERMINAL_LINE_LENGTH;
    }
    uint8_t remaining = cursor_characters_after;

    if (!cursor_is_absolute) {
        while (remaining > column && logical > 0U) {
            remaining = static_cast<uint8_t>(remaining - column);
            --logical;
            const uint8_t index = physical_index(logical);
            column = static_cast<uint8_t>(strlen(lines[index]));
        }
        if (remaining <= column) {
            column = static_cast<uint8_t>(column - remaining);
        } else {
            column = 0U;
        }
    }

    cursor_x = OUTPUT_LEFT + static_cast<int16_t>(column) * CHARACTER_WIDTH;
    cursor_y = OUTPUT_TOP + static_cast<int16_t>(logical) * LINE_HEIGHT + CURSOR_Y_OFFSET;
    display.Set_Draw_color(COLOR_TEXT);
    display.Fill_Rectangle(
        cursor_x, cursor_y, cursor_x + CURSOR_WIDTH - 1, cursor_y);
    cursor_drawn = true;
}

void start_new_line()
{
    if (line_count < TERMINAL_VISIBLE_LINES) {
        ++line_count;
    } else {
        first_line = static_cast<uint8_t>((first_line + 1U) % TERMINAL_VISIBLE_LINES);
        // Scrolling changes every logical-to-screen line mapping.
        full_redraw_needed = true;
    }

    const uint8_t index = physical_index(static_cast<uint8_t>(line_count - 1U));
    lines[index][0] = '\0';
    current_length = 0U;
}

void mark_line_dirty(uint8_t logical_index, uint8_t from_column)
{
    const uint32_t mask = static_cast<uint32_t>(1UL) << logical_index;
    if ((dirty_lines & mask) == 0U || from_column < dirty_from[logical_index]) {
        dirty_from[logical_index] = from_column;
    }
    dirty_lines |= mask;
}

void draw_terminal_line(uint8_t logical_index, uint8_t from_column, bool erase_remainder)
{
    const int16_t y = OUTPUT_TOP + static_cast<int16_t>(logical_index) * LINE_HEIGHT;
    const int16_t x = OUTPUT_LEFT + static_cast<int16_t>(from_column) * CHARACTER_WIDTH;

    if (logical_index >= line_count) {
        erase_remainder = true;
    }

    const uint8_t index = physical_index(logical_index);

    if (!erase_remainder) {
        display.Print_String(
            reinterpret_cast<uint8_t *>(&lines[index][from_column]), x, y);
        return;
    }

    // One opaque text pass both redraws the line and erases stale characters.
    // This avoids the visible flash caused by clearing the whole output area.
    char rendered_line[TERMINAL_LINE_LENGTH + 1U];
    uint8_t column = from_column;
    uint8_t rendered_column = 0U;
    while (logical_index < line_count && column < TERMINAL_LINE_LENGTH &&
           lines[index][column] != '\0') {
        rendered_line[rendered_column++] = lines[index][column++];
    }
    while (column++ < TERMINAL_LINE_LENGTH) {
        rendered_line[rendered_column++] = ' ';
    }
    rendered_line[rendered_column] = '\0';
    display.Print_String(reinterpret_cast<uint8_t *>(rendered_line), x, y);
}

void draw_frame()
{
    display.Fill_Screen(COLOR_BACKGROUND);
    display.Set_Draw_color(COLOR_FRAME);
    display.Draw_Rectangle(2, 2, SCREEN_WIDTH - 3, SCREEN_HEIGHT - 3);
}

void configure_terminal_text()
{
    display.Set_Text_Mode(0);
    display.Set_Text_colour(COLOR_TEXT);
    display.Set_Text_Back_colour(COLOR_BACKGROUND);
    display.Set_Text_Size(1);
}

void redraw_all()
{
    erase_cursor();
    draw_frame();
    configure_terminal_text();
    for (uint8_t logical = 0U; logical < TERMINAL_VISIBLE_LINES; ++logical) {
        draw_terminal_line(logical, 0U, true);
    }
    dirty_lines = 0UL;
    erase_remainder_lines = 0UL;
    full_redraw_needed = false;
    draw_cursor();
}

}  // namespace

void terminal_init()
{
    prepare_display_bus();
    display.Init_LCD();
    display.Set_Rotation(1);
    terminal_clear();
}

void terminal_clear()
{
    prepare_display_bus();
    first_line = 0U;
    line_count = 1U;
    current_length = 0U;
    dirty_lines = 0UL;
    erase_remainder_lines = 0UL;
    full_redraw_needed = false;
    cursor_drawn = false;
    cursor_is_absolute = false;
    for (uint8_t index = 0U; index < TERMINAL_VISIBLE_LINES; ++index) {
        lines[index][0] = '\0';
        dirty_from[index] = 0U;
    }
    draw_frame();
}

void terminal_write(char character)
{
    if (character == '\r') {
        return;
    }
    if (character == '\n') {
        start_new_line();
        return;
    }
    if (character < ' ') {
        return;
    }
    if (current_length >= TERMINAL_LINE_LENGTH) {
        start_new_line();
    }

    const uint8_t logical = static_cast<uint8_t>(line_count - 1U);
    const uint8_t index = physical_index(logical);
    mark_line_dirty(logical, current_length);
    lines[index][current_length++] = character;
    lines[index][current_length] = '\0';
}

void terminal_refresh()
{
    if (update_depth > 0U) {
        return;
    }

    prepare_display_bus();
    erase_cursor();
    configure_terminal_text();

    if (full_redraw_needed) {
        for (uint8_t logical = 0U; logical < TERMINAL_VISIBLE_LINES; ++logical) {
            draw_terminal_line(logical, 0U, true);
        }
    } else {
        for (uint8_t logical = 0U; logical < line_count; ++logical) {
            const uint32_t mask = static_cast<uint32_t>(1UL) << logical;
            if ((dirty_lines & mask) != 0U) {
                draw_terminal_line(
                    logical, dirty_from[logical], (erase_remainder_lines & mask) != 0U);
            }
        }
    }

    dirty_lines = 0UL;
    erase_remainder_lines = 0UL;
    full_redraw_needed = false;
    draw_cursor();
}

void terminal_print(const char *text)
{
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        terminal_write(*text++);
    }
    terminal_refresh();
}

void terminal_println(const char *text)
{
    if (text != nullptr) {
        while (*text != '\0') {
            terminal_write(*text++);
        }
    }
    terminal_write('\n');
    terminal_refresh();
}

void terminal_show_command(const char *command)
{
    terminal_print(SHELL_PROMPT);
    terminal_println(command == nullptr ? "" : command);
}

void terminal_backspace()
{
    if (current_length == 0U) {
        if (line_count <= 1U) {
            return;
        }

        // Move back across a line created by command wrapping. A line already
        // scrolled off-screen is intentionally not restored.
        const uint8_t old_index = physical_index(static_cast<uint8_t>(line_count - 1U));
        lines[old_index][0] = '\0';
        --line_count;
        const uint8_t previous_index = physical_index(static_cast<uint8_t>(line_count - 1U));
        current_length = static_cast<uint8_t>(strlen(lines[previous_index]));
        full_redraw_needed = true;
    }

    if (current_length > 0U) {
        --current_length;
        const uint8_t logical = static_cast<uint8_t>(line_count - 1U);
        const uint8_t index = physical_index(logical);
        lines[index][current_length] = '\0';
        mark_line_dirty(logical, current_length);
        erase_remainder_lines |= static_cast<uint32_t>(1UL) << logical;
    }
    terminal_refresh();
}

void terminal_set_cursor(uint8_t characters_after_cursor)
{
    cursor_enabled = true;
    cursor_is_absolute = false;
    cursor_characters_after = characters_after_cursor;
    if (update_depth == 0U) {
        prepare_display_bus();
        erase_cursor();
        draw_cursor();
    }
}

uint32_t terminal_render_screen(
    TerminalScreenLineProvider provider,
    void *context,
    uint8_t cursor_row,
    uint8_t cursor_column)
{
    prepare_display_bus();
    erase_cursor();

    const bool mapping_changed = first_line != 0U ||
        line_count != TERMINAL_VISIBLE_LINES;
    first_line = 0U;
    line_count = TERMINAL_VISIBLE_LINES;
    uint32_t changed_rows = 0UL;

    for (uint8_t row = 0U; row < TERMINAL_VISIBLE_LINES; ++row) {
        char replacement[TERMINAL_LINE_LENGTH + 1U];
        replacement[0] = '\0';
        if (provider != nullptr) {
            provider(row, replacement, sizeof(replacement), context);
        }
        replacement[TERMINAL_LINE_LENGTH] = '\0';

        uint8_t first_change = 0U;
        if (!mapping_changed) {
            while (first_change < TERMINAL_LINE_LENGTH &&
                   lines[row][first_change] == replacement[first_change] &&
                   replacement[first_change] != '\0') {
                ++first_change;
            }
            if (lines[row][first_change] == replacement[first_change]) {
                continue;
            }
        }

        strncpy(lines[row], replacement, TERMINAL_LINE_LENGTH);
        lines[row][TERMINAL_LINE_LENGTH] = '\0';
        mark_line_dirty(row, first_change);
        erase_remainder_lines |= static_cast<uint32_t>(1UL) << row;
        changed_rows |= static_cast<uint32_t>(1UL) << row;
    }

    current_length = static_cast<uint8_t>(
        strlen(lines[TERMINAL_VISIBLE_LINES - 1U]));
    cursor_enabled = true;
    cursor_is_absolute = true;
    absolute_cursor_row = cursor_row;
    absolute_cursor_column = cursor_column;
    terminal_refresh();
    return changed_rows;
}

void terminal_hide_cursor()
{
    cursor_enabled = false;
    if (update_depth == 0U) {
        prepare_display_bus();
        erase_cursor();
    }
}

void terminal_redraw()
{
    prepare_display_bus();
    redraw_all();
}

void terminal_demo_fill(uint16_t color)
{
    prepare_display_bus();
    erase_cursor();
    display.Fill_Screen(color);
    delay(350U);
    redraw_all();
}

void terminal_run_benchmark(TerminalBenchmark *result)
{
    if (result == nullptr) {
        return;
    }

    prepare_display_bus();
    erase_cursor();
    uint32_t started = micros();
    display.Fill_Screen(COLOR_BACKGROUND);
    result->full_screen_us = static_cast<uint32_t>(micros() - started);

    started = micros();
    redraw_all();
    result->terminal_redraw_us = static_cast<uint32_t>(micros() - started);

    configure_terminal_text();
    started = micros();
    // The explicit uint8_t type selects LCDWIKI's C-string overload. Passing a
    // string literal directly would select its heap-allocating String overload.
    display.Print_String(benchmark_text, OUTPUT_LEFT, OUTPUT_TOP);
    result->text_line_us = static_cast<uint32_t>(micros() - started);

    // The representative line overwrote row zero; always restore the ring.
    redraw_all();
}

void terminal_begin_update()
{
    if (update_depth < UINT8_MAX) {
        ++update_depth;
    }
}

void terminal_end_update()
{
    if (update_depth == 0U) {
        return;
    }
    --update_depth;
    if (update_depth == 0U) {
        terminal_refresh();
    }
}
