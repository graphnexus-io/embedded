#include "text_editor.h"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "config.h"
#include "filesystem.h"
#include "tft_terminal.h"

namespace {

enum class EditorInputState : uint8_t {
    Normal,
    Escape,
    Csi,
    Ss3,
};

enum class EditorNotice : uint8_t {
    None,
    Saved,
    BufferFull,
    SaveNotReady,
    SaveParentMissing,
    SaveOpenFailed,
    SaveWriteFailed,
    SaveVerifyFailed,
    SaveFailed,
};

constexpr uint16_t ESCAPE_TIMEOUT_MS = 35U;

char text_buffer[TEXT_EDITOR_MAX_BYTES + 1U];
char editor_path[FS_MAX_PATH + 1U];
uint16_t text_length = 0U;
uint16_t cursor_position = 0U;
uint16_t first_visible_line = 0U;
uint8_t preferred_column = 0U;
bool preferred_column_valid = false;
bool editor_is_active = false;
bool file_is_new = false;
bool buffer_is_dirty = false;
bool discard_is_armed = false;
bool serial_screen_initialized = false;
bool previous_was_cr = false;
EditorInputState input_state = EditorInputState::Normal;
uint8_t csi_parameter = 0U;
uint32_t escape_started_ms = 0UL;
EditorNotice notice = EditorNotice::None;
TextEditorExitResult exit_result = TEXT_EDITOR_EXIT_NONE;

void append_ram(char *output, uint8_t capacity, uint8_t *length, const char *text)
{
    if (output == nullptr || length == nullptr || text == nullptr || capacity == 0U) {
        return;
    }
    while (*text != '\0' && *length < static_cast<uint8_t>(capacity - 1U)) {
        output[(*length)++] = *text++;
    }
    output[*length] = '\0';
}

void append_progmem(char *output, uint8_t capacity, uint8_t *length, PGM_P text)
{
    if (output == nullptr || length == nullptr || text == nullptr || capacity == 0U) {
        return;
    }
    char character = static_cast<char>(pgm_read_byte(text));
    while (character != '\0' && *length < static_cast<uint8_t>(capacity - 1U)) {
        output[(*length)++] = character;
        ++text;
        character = static_cast<char>(pgm_read_byte(text));
    }
    output[*length] = '\0';
}

void cursor_coordinates(uint16_t position, uint16_t *line, uint8_t *column)
{
    uint16_t visual_line = 0U;
    uint8_t visual_column = 0U;
    for (uint16_t index = 0U; index < position; ++index) {
        if (text_buffer[index] == '\n') {
            ++visual_line;
            visual_column = 0U;
            continue;
        }
        if (visual_column >= TERMINAL_LINE_LENGTH) {
            ++visual_line;
            visual_column = 0U;
        }
        ++visual_column;
    }

    // A cursor immediately before a character that soft-wraps belongs at the
    // start of the following visual row, not beyond the previous row's edge.
    if (visual_column >= TERMINAL_LINE_LENGTH && position < text_length &&
        text_buffer[position] != '\n') {
        ++visual_line;
        visual_column = 0U;
    }
    *line = visual_line;
    *column = visual_column;
}

bool find_position_on_visual_line(
    uint16_t target_line, uint8_t target_column, uint16_t *position)
{
    uint16_t visual_line = 0U;
    uint8_t visual_column = 0U;
    bool found = false;
    uint16_t best_position = 0U;

    for (uint16_t candidate = 0U; candidate <= text_length; ++candidate) {
        uint16_t candidate_line = visual_line;
        uint8_t candidate_column = visual_column;
        if (candidate_column >= TERMINAL_LINE_LENGTH && candidate < text_length &&
            text_buffer[candidate] != '\n') {
            ++candidate_line;
            candidate_column = 0U;
        }

        if (candidate_line == target_line) {
            if (!found || candidate_column <= target_column) {
                found = true;
                best_position = candidate;
            }
            if (candidate_column >= target_column) {
                *position = candidate;
                return true;
            }
        } else if (candidate_line > target_line) {
            break;
        }

        if (candidate == text_length) {
            break;
        }
        if (text_buffer[candidate] == '\n') {
            ++visual_line;
            visual_column = 0U;
        } else {
            if (visual_column >= TERMINAL_LINE_LENGTH) {
                ++visual_line;
                visual_column = 0U;
            }
            ++visual_column;
        }
    }

    if (found) {
        *position = best_position;
    }
    return found;
}

void fill_content_line(uint16_t target_line, char *output, uint8_t capacity)
{
    if (capacity == 0U) {
        return;
    }
    output[0] = '\0';
    uint16_t visual_line = 0U;
    uint8_t visual_column = 0U;

    for (uint16_t index = 0U; index < text_length; ++index) {
        const char character = text_buffer[index];
        if (character == '\n') {
            if (visual_line == target_line) {
                return;
            }
            ++visual_line;
            visual_column = 0U;
            continue;
        }
        if (visual_column >= TERMINAL_LINE_LENGTH) {
            if (visual_line == target_line) {
                return;
            }
            ++visual_line;
            visual_column = 0U;
        }
        if (visual_line == target_line && visual_column < capacity - 1U) {
            output[visual_column] = character;
            output[visual_column + 1U] = '\0';
        }
        ++visual_column;
    }
}

void fill_footer(char *line, uint8_t capacity)
{
    uint8_t length = 0U;
    line[0] = '\0';
    if (discard_is_armed) {
        append_progmem(
            line, capacity, &length,
            PSTR("Unsaved: ^O save | Esc discard | key cancels"));
        return;
    }

    switch (notice) {
        case EditorNotice::Saved:
            append_progmem(line, capacity, &length, PSTR("Saved | "));
            break;
        case EditorNotice::BufferFull:
            append_progmem(line, capacity, &length, PSTR("Buffer full (1024 bytes) | "));
            break;
        case EditorNotice::SaveNotReady:
            append_progmem(line, capacity, &length, PSTR("Save failed: SD not ready | "));
            break;
        case EditorNotice::SaveParentMissing:
            append_progmem(line, capacity, &length, PSTR("Save failed: parent missing | "));
            break;
        case EditorNotice::SaveOpenFailed:
            append_progmem(line, capacity, &length, PSTR("Save failed: open error | "));
            break;
        case EditorNotice::SaveWriteFailed:
            append_progmem(line, capacity, &length, PSTR("Save failed: write error | "));
            break;
        case EditorNotice::SaveVerifyFailed:
            append_progmem(line, capacity, &length, PSTR("Save failed: verify error | "));
            break;
        case EditorNotice::SaveFailed:
            append_progmem(line, capacity, &length, PSTR("Save failed | "));
            break;
        case EditorNotice::None:
            break;
    }
    append_progmem(line, capacity, &length, PSTR("^O Save | Esc Exit | Arrows Move"));
}

void editor_line_provider(uint8_t row, char *line, uint8_t capacity, void *)
{
    if (capacity == 0U) {
        return;
    }
    line[0] = '\0';
    if (row == 0U) {
        uint8_t length = 0U;
        append_progmem(line, capacity, &length, PSTR("nano "));
        append_ram(line, capacity, &length, editor_path);
        if (buffer_is_dirty) {
            append_progmem(line, capacity, &length, PSTR(" *"));
        }
        if (file_is_new) {
            append_progmem(line, capacity, &length, PSTR(" [new]"));
        }
        return;
    }
    if (row == TERMINAL_VISIBLE_LINES - 1U) {
        fill_footer(line, capacity);
        return;
    }
    fill_content_line(
        static_cast<uint16_t>(first_visible_line + row - 1U), line, capacity);
}

void serial_position_cursor(uint8_t row, uint8_t column)
{
    Serial.print(F("\x1b["));
    Serial.print(static_cast<uint16_t>(row) + 1U);
    Serial.write(';');
    Serial.print(static_cast<uint16_t>(column) + 1U);
    Serial.write('H');
}

void render_editor()
{
    uint16_t cursor_line = 0U;
    uint8_t cursor_column = 0U;
    cursor_coordinates(cursor_position, &cursor_line, &cursor_column);
    if (cursor_line < first_visible_line) {
        first_visible_line = cursor_line;
    } else if (cursor_line >= first_visible_line + TEXT_EDITOR_CONTENT_ROWS) {
        first_visible_line = static_cast<uint16_t>(
            cursor_line - TEXT_EDITOR_CONTENT_ROWS + 1U);
    }
    const uint8_t screen_cursor_row = static_cast<uint8_t>(
        cursor_line - first_visible_line + 1U);

    uint32_t changed_rows = terminal_render_screen(
        editor_line_provider, nullptr, screen_cursor_row, cursor_column);
    if (!serial_screen_initialized) {
        Serial.print(F("\x1b[2J\x1b[H"));
        serial_screen_initialized = true;
        changed_rows = (static_cast<uint32_t>(1UL) << TERMINAL_VISIBLE_LINES) - 1UL;
    }

    for (uint8_t row = 0U; row < TERMINAL_VISIBLE_LINES; ++row) {
        if ((changed_rows & (static_cast<uint32_t>(1UL) << row)) == 0U) {
            continue;
        }
        char line[TERMINAL_LINE_LENGTH + 1U];
        editor_line_provider(row, line, sizeof(line), nullptr);
        serial_position_cursor(row, 0U);
        Serial.print(F("\x1b[2K"));
        Serial.print(line);
    }
    serial_position_cursor(screen_cursor_row, cursor_column);
}

void clear_transient_notice()
{
    notice = EditorNotice::None;
    discard_is_armed = false;
}

void insert_character(char character)
{
    if (text_length >= TEXT_EDITOR_MAX_BYTES) {
        notice = EditorNotice::BufferFull;
        discard_is_armed = false;
        render_editor();
        return;
    }
    clear_transient_notice();
    memmove(
        &text_buffer[cursor_position + 1U],
        &text_buffer[cursor_position],
        static_cast<size_t>(text_length - cursor_position + 1U));
    text_buffer[cursor_position++] = character;
    ++text_length;
    buffer_is_dirty = true;
    preferred_column_valid = false;
    render_editor();
}

void backspace_character()
{
    if (cursor_position == 0U) {
        return;
    }
    clear_transient_notice();
    --cursor_position;
    memmove(
        &text_buffer[cursor_position],
        &text_buffer[cursor_position + 1U],
        static_cast<size_t>(text_length - cursor_position));
    --text_length;
    buffer_is_dirty = true;
    preferred_column_valid = false;
    render_editor();
}

void delete_character()
{
    if (cursor_position >= text_length) {
        return;
    }
    clear_transient_notice();
    memmove(
        &text_buffer[cursor_position],
        &text_buffer[cursor_position + 1U],
        static_cast<size_t>(text_length - cursor_position));
    --text_length;
    buffer_is_dirty = true;
    preferred_column_valid = false;
    render_editor();
}

void move_horizontal(int8_t direction)
{
    clear_transient_notice();
    if (direction < 0 && cursor_position > 0U) {
        --cursor_position;
    } else if (direction > 0 && cursor_position < text_length) {
        ++cursor_position;
    }
    preferred_column_valid = false;
    render_editor();
}

void move_vertical(int8_t direction)
{
    clear_transient_notice();
    uint16_t current_line = 0U;
    uint8_t current_column = 0U;
    cursor_coordinates(cursor_position, &current_line, &current_column);
    if (!preferred_column_valid) {
        preferred_column = current_column;
        preferred_column_valid = true;
    }

    if (direction < 0) {
        if (current_line == 0U) {
            render_editor();
            return;
        }
        --current_line;
    } else {
        ++current_line;
    }

    uint16_t new_position = cursor_position;
    if (find_position_on_visual_line(current_line, preferred_column, &new_position)) {
        cursor_position = new_position;
    }
    render_editor();
}

void save_file()
{
    discard_is_armed = false;
    const FilesystemStatus status = filesystem_write_text_file(
        editor_path, text_buffer, text_length);
    switch (status) {
        case FILESYSTEM_OK:
            buffer_is_dirty = false;
            file_is_new = false;
            notice = EditorNotice::Saved;
            break;
        case FILESYSTEM_NOT_READY:
            notice = EditorNotice::SaveNotReady;
            break;
        case FILESYSTEM_PARENT_NOT_FOUND:
        case FILESYSTEM_PARENT_NOT_DIRECTORY:
            notice = EditorNotice::SaveParentMissing;
            break;
        case FILESYSTEM_OPEN_FAILED:
            notice = EditorNotice::SaveOpenFailed;
            break;
        case FILESYSTEM_WRITE_FAILED:
        case FILESYSTEM_REMOVE_FAILED:
            notice = EditorNotice::SaveWriteFailed;
            break;
        case FILESYSTEM_VERIFY_FAILED:
            notice = EditorNotice::SaveVerifyFailed;
            break;
        default:
            notice = EditorNotice::SaveFailed;
            break;
    }
    render_editor();
}

void handle_standalone_escape()
{
    input_state = EditorInputState::Normal;
    if (buffer_is_dirty && !discard_is_armed) {
        discard_is_armed = true;
        notice = EditorNotice::None;
        render_editor();
        return;
    }
    exit_result = discard_is_armed ?
        TEXT_EDITOR_EXIT_DISCARDED : TEXT_EDITOR_EXIT_CLOSED;
    editor_is_active = false;
}

void handle_escape_sequence(char character)
{
    if (input_state == EditorInputState::Escape) {
        if (character == '[') {
            input_state = EditorInputState::Csi;
            csi_parameter = 0U;
        } else if (character == 'O') {
            input_state = EditorInputState::Ss3;
        } else {
            handle_standalone_escape();
        }
        return;
    }

    if (input_state == EditorInputState::Ss3) {
        if (character == 'A') {
            move_vertical(-1);
        } else if (character == 'B') {
            move_vertical(1);
        } else if (character == 'C') {
            move_horizontal(1);
        } else if (character == 'D') {
            move_horizontal(-1);
        }
        input_state = EditorInputState::Normal;
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
        move_vertical(-1);
    } else if (character == 'B') {
        move_vertical(1);
    } else if (character == 'C') {
        move_horizontal(1);
    } else if (character == 'D') {
        move_horizontal(-1);
    } else if (character == '~' && csi_parameter == 3U) {
        delete_character();
    }
    input_state = EditorInputState::Normal;
}

}  // namespace

FilesystemStatus text_editor_open(const char *normalized_path)
{
    bool new_file = false;
    uint16_t loaded_length = 0U;
    const FilesystemStatus status = filesystem_open_text_for_edit(
        normalized_path,
        text_buffer,
        sizeof(text_buffer),
        &loaded_length,
        &new_file);
    if (status != FILESYSTEM_OK) {
        return status;
    }

    strncpy(editor_path, normalized_path, FS_MAX_PATH);
    editor_path[FS_MAX_PATH] = '\0';
    text_length = loaded_length;
    cursor_position = 0U;
    first_visible_line = 0U;
    preferred_column = 0U;
    preferred_column_valid = false;
    editor_is_active = true;
    file_is_new = new_file;
    buffer_is_dirty = false;
    discard_is_armed = false;
    serial_screen_initialized = false;
    previous_was_cr = false;
    input_state = EditorInputState::Normal;
    csi_parameter = 0U;
    notice = EditorNotice::None;
    exit_result = TEXT_EDITOR_EXIT_NONE;
    render_editor();
    return FILESYSTEM_OK;
}

bool text_editor_active()
{
    return editor_is_active;
}

void text_editor_expect_crlf_tail()
{
    // nano may be entered by the CR half of a CRLF command terminator. Treat
    // the following LF as part of that terminator, not as the first edit.
    previous_was_cr = true;
}

void text_editor_poll()
{
    while (editor_is_active && Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (input_state != EditorInputState::Normal) {
            handle_escape_sequence(character);
            continue;
        }
        if (character == 0x1B) {
            input_state = EditorInputState::Escape;
            escape_started_ms = millis();
            continue;
        }
        if (character == 0x13 || character == 0x0F) {
            save_file();
            previous_was_cr = false;
            continue;
        }
        if (character == '\r') {
            insert_character('\n');
            previous_was_cr = true;
            continue;
        }
        if (character == '\n') {
            if (!previous_was_cr) {
                insert_character('\n');
            }
            previous_was_cr = false;
            continue;
        }

        previous_was_cr = false;
        if (character == '\b' || character == 0x7F) {
            backspace_character();
        } else if (character >= ' ' && character <= '~') {
            insert_character(character);
        }
    }

    if (editor_is_active && input_state == EditorInputState::Escape &&
        static_cast<uint32_t>(millis() - escape_started_ms) >= ESCAPE_TIMEOUT_MS) {
        handle_standalone_escape();
    }
}

TextEditorExitResult text_editor_take_exit_result()
{
    const TextEditorExitResult result = exit_result;
    exit_result = TEXT_EDITOR_EXIT_NONE;
    return result;
}
