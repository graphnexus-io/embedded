#include "command_shell.h"

#include <Arduino.h>
#include <ctype.h>
#include <string.h>

#include "commands.h"
#include "config.h"
#include "console.h"
#include "text_editor.h"
#include "tft_terminal.h"

namespace {

enum class InputState : uint8_t {
    Normal,
    Escape,
    Csi,
    Ss3,
};

enum ParseResult : uint8_t {
    PARSE_OK = 0,
    PARSE_UNMATCHED_QUOTE,
    PARSE_TOO_MANY_ARGUMENTS,
};

char input_buffer[COMMAND_BUFFER_SIZE];
uint8_t input_length = 0U;
uint8_t cursor_position = 0U;
bool input_overflow = false;
bool previous_was_cr = false;
InputState input_state = InputState::Normal;
uint8_t csi_parameter = 0U;
CommandStatus last_status = COMMAND_OK;

char history_entries[HISTORY_ENTRY_COUNT][HISTORY_ENTRY_SIZE];
uint8_t history_first = 0U;
uint8_t history_count = 0U;
int8_t history_navigation = -1;
char saved_input[COMMAND_BUFFER_SIZE];

uint8_t history_physical_index(uint8_t chronological_index)
{
    return static_cast<uint8_t>((history_first + chronological_index) % HISTORY_ENTRY_COUNT);
}

bool has_non_whitespace(const char *text)
{
    while (*text != '\0') {
        if (isspace(static_cast<unsigned char>(*text)) == 0) {
            return true;
        }
        ++text;
    }
    return false;
}

void history_add(const char *line)
{
    if (!has_non_whitespace(line)) {
        return;
    }
    if (history_count > 0U &&
        strcmp(line, history_entries[history_physical_index(history_count - 1U)]) == 0) {
        return;
    }

    uint8_t target;
    if (history_count < HISTORY_ENTRY_COUNT) {
        target = history_physical_index(history_count);
        ++history_count;
    } else {
        target = history_first;
        history_first = static_cast<uint8_t>((history_first + 1U) % HISTORY_ENTRY_COUNT);
    }
    strncpy(history_entries[target], line, HISTORY_ENTRY_SIZE - 1U);
    history_entries[target][HISTORY_ENTRY_SIZE - 1U] = '\0';
}

void print_prompt()
{
    Serial.print(F(SHELL_PROMPT));
    terminal_print(SHELL_PROMPT);
    terminal_set_cursor(0U);
}

void serial_cursor_left(uint8_t count)
{
    while (count-- > 0U) {
        Serial.print(F("\x1b[D"));
    }
}

void serial_cursor_right()
{
    Serial.print(F("\x1b[C"));
}

void redraw_tft_input(uint8_t previous_length)
{
    while (previous_length-- > 0U) {
        terminal_backspace();
    }
    for (uint8_t index = 0U; index < input_length; ++index) {
        terminal_write(input_buffer[index]);
    }
    terminal_set_cursor(static_cast<uint8_t>(input_length - cursor_position));
}

void leave_history_navigation()
{
    history_navigation = -1;
}

void replace_input_line(const char *replacement)
{
    const uint8_t previous_length = input_length;
    strncpy(input_buffer, replacement, COMMAND_BUFFER_SIZE - 1U);
    input_buffer[COMMAND_BUFFER_SIZE - 1U] = '\0';
    input_length = static_cast<uint8_t>(strlen(input_buffer));
    cursor_position = input_length;
    input_overflow = false;

    Serial.write('\r');
    Serial.print(F(SHELL_PROMPT));
    Serial.print(input_buffer);
    if (previous_length > input_length) {
        const uint8_t stale = static_cast<uint8_t>(previous_length - input_length);
        for (uint8_t index = 0U; index < stale; ++index) {
            Serial.write(' ');
        }
        serial_cursor_left(stale);
    }
    redraw_tft_input(previous_length);
}

void history_older()
{
    if (history_count == 0U) {
        return;
    }
    if (history_navigation < 0) {
        strncpy(saved_input, input_buffer, COMMAND_BUFFER_SIZE);
        saved_input[COMMAND_BUFFER_SIZE - 1U] = '\0';
        history_navigation = static_cast<int8_t>(history_count - 1U);
    } else if (history_navigation > 0) {
        --history_navigation;
    }
    replace_input_line(history_entries[history_physical_index(
        static_cast<uint8_t>(history_navigation))]);
}

void history_newer()
{
    if (history_navigation < 0) {
        return;
    }
    if (history_navigation < static_cast<int8_t>(history_count - 1U)) {
        ++history_navigation;
        replace_input_line(history_entries[history_physical_index(
            static_cast<uint8_t>(history_navigation))]);
    } else {
        history_navigation = -1;
        replace_input_line(saved_input);
    }
}

void move_cursor_left()
{
    if (cursor_position == 0U) {
        return;
    }
    --cursor_position;
    serial_cursor_left(1U);
    terminal_set_cursor(static_cast<uint8_t>(input_length - cursor_position));
}

void move_cursor_right()
{
    if (cursor_position >= input_length) {
        return;
    }
    ++cursor_position;
    serial_cursor_right();
    terminal_set_cursor(static_cast<uint8_t>(input_length - cursor_position));
}

void repaint_serial_tail(uint8_t from)
{
    for (uint8_t index = from; index < input_length; ++index) {
        Serial.write(input_buffer[index]);
    }
    Serial.write(' ');
    serial_cursor_left(static_cast<uint8_t>(input_length - from + 1U));
}

void backspace_at_cursor()
{
    if (cursor_position == 0U) {
        return;
    }
    leave_history_navigation();

    const uint8_t previous_length = input_length;
    --cursor_position;
    for (uint8_t index = cursor_position; index < (input_length - 1U); ++index) {
        input_buffer[index] = input_buffer[index + 1U];
    }
    --input_length;
    input_buffer[input_length] = '\0';

    Serial.write('\b');
    repaint_serial_tail(cursor_position);
    if (cursor_position == input_length) {
        terminal_backspace();
        terminal_set_cursor(0U);
    } else {
        redraw_tft_input(previous_length);
    }
}

void delete_at_cursor()
{
    if (cursor_position >= input_length) {
        return;
    }
    leave_history_navigation();

    const uint8_t previous_length = input_length;
    for (uint8_t index = cursor_position; index < (input_length - 1U); ++index) {
        input_buffer[index] = input_buffer[index + 1U];
    }
    --input_length;
    input_buffer[input_length] = '\0';
    repaint_serial_tail(cursor_position);
    redraw_tft_input(previous_length);
}

void insert_character(char character)
{
    if (input_length >= (COMMAND_BUFFER_SIZE - 1U)) {
        input_overflow = true;
        return;
    }
    leave_history_navigation();

    const uint8_t previous_length = input_length;
    const bool append = cursor_position == input_length;
    for (uint8_t index = input_length; index > cursor_position; --index) {
        input_buffer[index] = input_buffer[index - 1U];
    }
    input_buffer[cursor_position++] = character;
    ++input_length;
    input_buffer[input_length] = '\0';

    Serial.write(character);
    if (!append) {
        for (uint8_t index = cursor_position; index < input_length; ++index) {
            Serial.write(input_buffer[index]);
        }
        serial_cursor_left(static_cast<uint8_t>(input_length - cursor_position));
        redraw_tft_input(previous_length);
    } else {
        terminal_write(character);
        terminal_set_cursor(0U);
    }
}

void handle_escape_character(char character)
{
    if (input_state == InputState::Escape) {
        if (character == '[') {
            input_state = InputState::Csi;
            csi_parameter = 0U;
        } else if (character == 'O') {
            input_state = InputState::Ss3;
        } else {
            input_state = InputState::Normal;
        }
        return;
    }

    if (input_state == InputState::Ss3) {
        if (character == 'A') {
            history_older();
        } else if (character == 'B') {
            history_newer();
        } else if (character == 'C') {
            move_cursor_right();
        } else if (character == 'D') {
            move_cursor_left();
        }
        input_state = InputState::Normal;
        return;
    }

    if (character >= '0' && character <= '9') {
        const uint8_t digit = static_cast<uint8_t>(character - '0');
        if (csi_parameter <= 25U) {
            csi_parameter = static_cast<uint8_t>(csi_parameter * 10U + digit);
        }
        return;
    }
    if (character == ';') {
        return;
    }

    if (character == 'A') {
        history_older();
    } else if (character == 'B') {
        history_newer();
    } else if (character == 'C') {
        move_cursor_right();
    } else if (character == 'D') {
        move_cursor_left();
    } else if (character == '~' && csi_parameter == 3U) {
        delete_at_cursor();
    }
    input_state = InputState::Normal;
}

ParseResult tokenize(char *text, int *argc, char *argv[])
{
    char *read = text;
    int argument_count = 0;

    while (true) {
        while (*read != '\0' && isspace(static_cast<unsigned char>(*read)) != 0) {
            ++read;
        }
        if (*read == '\0') {
            break;
        }
        if (argument_count >= COMMAND_MAX_ARGUMENTS) {
            return PARSE_TOO_MANY_ARGUMENTS;
        }

        char *write = read;
        argv[argument_count++] = write;
        bool quoted = false;
        while (*read != '\0') {
            const char character = *read;
            if (character == '"') {
                quoted = !quoted;
                ++read;
                continue;
            }
            if (!quoted && isspace(static_cast<unsigned char>(character)) != 0) {
                break;
            }
            *write++ = character;
            ++read;
        }
        if (quoted) {
            return PARSE_UNMATCHED_QUOTE;
        }

        const bool had_separator = *read != '\0';
        *write = '\0';
        if (had_separator) {
            ++read;
        }
    }

    *argc = argument_count;
    return PARSE_OK;
}

CommandStatus execute_section(char *section)
{
    char *argv[COMMAND_MAX_ARGUMENTS];
    int argc = 0;
    const ParseResult result = tokenize(section, &argc, argv);
    if (result == PARSE_UNMATCHED_QUOTE) {
        console_println(F("Syntax error: unmatched quote"));
        return COMMAND_BAD_ARGUMENTS;
    }
    if (result == PARSE_TOO_MANY_ARGUMENTS) {
        console_println(F("Error: too many arguments"));
        return COMMAND_BAD_ARGUMENTS;
    }
    if (argc == 0) {
        return COMMAND_OK;
    }
    return commands_dispatch(argc, argv);
}

CommandStatus execute_line(char *line)
{
    char *section = line;
    bool quoted = false;
    uint8_t command_count = 0U;
    CommandStatus status = COMMAND_OK;

    for (char *cursor = line;; ++cursor) {
        if (*cursor == '"') {
            quoted = !quoted;
        }
        if ((*cursor == ';' && !quoted) || *cursor == '\0') {
            const bool at_end = *cursor == '\0';
            *cursor = '\0';
            if (has_non_whitespace(section)) {
                if (command_count >= COMMANDS_PER_LINE) {
                    console_println(F("Error: too many commands"));
                    return COMMAND_BAD_ARGUMENTS;
                }
                ++command_count;
                status = execute_section(section);
                if (text_editor_active()) {
                    return status;
                }
            }
            if (at_end) {
                break;
            }
            section = cursor + 1;
        }
    }
    return status;
}

void submit_line()
{
    Serial.print(F("\r\n"));
    terminal_begin_update();
    terminal_hide_cursor();
    terminal_println("");
    input_buffer[input_length] = '\0';

    if (input_overflow) {
        console_println(F("Error: command line too long"));
        last_status = COMMAND_BAD_ARGUMENTS;
    } else {
        history_add(input_buffer);
        last_status = execute_line(input_buffer);
    }

    input_length = 0U;
    cursor_position = 0U;
    input_overflow = false;
    input_buffer[0] = '\0';
    history_navigation = -1;
    if (!text_editor_active()) {
        print_prompt();
    }
    terminal_end_update();
}

void resume_after_editor()
{
    Serial.print(F("\x1b[2J\x1b[H"));
    terminal_clear();
    const TextEditorExitResult result = text_editor_take_exit_result();
    if (result == TEXT_EDITOR_EXIT_DISCARDED) {
        console_println(F("Changes discarded"));
    } else {
        console_println(F("Editor closed"));
    }
    print_prompt();
}

}  // namespace

void shell_init()
{
    input_length = 0U;
    cursor_position = 0U;
    input_overflow = false;
    input_buffer[0] = '\0';
    previous_was_cr = false;
    input_state = InputState::Normal;
    csi_parameter = 0U;
    history_first = 0U;
    history_count = 0U;
    history_navigation = -1;
    saved_input[0] = '\0';
    last_status = COMMAND_OK;
    print_prompt();
}

void shell_poll()
{
    if (text_editor_active()) {
        text_editor_poll();
        if (!text_editor_active()) {
            resume_after_editor();
        }
        return;
    }
    if (Serial.available() <= 0) {
        return;
    }

    terminal_begin_update();
    while (Serial.available() > 0) {
        if (text_editor_active()) {
            // The Enter key that launched nano has been consumed; preserve any
            // following bytes so only the modal editor can interpret them.
            terminal_end_update();
            return;
        }
        const char character = static_cast<char>(Serial.read());

        if (input_state != InputState::Normal) {
            handle_escape_character(character);
            continue;
        }
        if (character == 0x1B) {
            input_state = InputState::Escape;
            continue;
        }
        if (character == '\r') {
            submit_line();
            previous_was_cr = true;
            if (text_editor_active()) {
                text_editor_expect_crlf_tail();
            }
            continue;
        }
        if (character == '\n') {
            if (!previous_was_cr) {
                submit_line();
            }
            previous_was_cr = false;
            continue;
        }

        previous_was_cr = false;
        if (character == '\b' || character == 0x7F) {
            backspace_at_cursor();
            continue;
        }
        if (character >= ' ' && character <= '~') {
            insert_character(character);
        }
    }
    terminal_end_update();
}

uint8_t shell_history_count()
{
    return history_count;
}

const char *shell_history_get(uint8_t chronological_index)
{
    if (chronological_index >= history_count) {
        return "";
    }
    return history_entries[history_physical_index(chronological_index)];
}

CommandStatus shell_last_status()
{
    return last_status;
}
