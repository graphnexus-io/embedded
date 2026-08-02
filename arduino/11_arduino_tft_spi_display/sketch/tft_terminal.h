#ifndef MINIOS_TFT_TERMINAL_H
#define MINIOS_TFT_TERMINAL_H

#include <Arduino.h>

struct TerminalBenchmark {
    uint32_t full_screen_us;
    uint32_t terminal_redraw_us;
    uint32_t text_line_us;
};

using TerminalScreenLineProvider = void (*)(
    uint8_t row, char *line, uint8_t capacity, void *context);

void terminal_init();
void terminal_clear();
void terminal_print(const char *text);
void terminal_println(const char *text);
void terminal_show_command(const char *command);
void terminal_backspace();
void terminal_set_cursor(uint8_t characters_after_cursor);
// Replace the 30-row terminal view without clearing the panel. The provider is
// called once per row; only rows whose text changed are physically redrawn.
// The returned bit mask identifies changed rows for a mirrored ANSI terminal.
uint32_t terminal_render_screen(
    TerminalScreenLineProvider provider,
    void *context,
    uint8_t cursor_row,
    uint8_t cursor_column);
void terminal_hide_cursor();
void terminal_redraw();
void terminal_demo_fill(uint16_t color);
void terminal_run_benchmark(TerminalBenchmark *result);

// Defer physical drawing while a command emits several output fragments.
void terminal_begin_update();
void terminal_end_update();

// Used by the flash-string console path to avoid an SRAM copy.
void terminal_write(char character);
void terminal_refresh();

#endif
