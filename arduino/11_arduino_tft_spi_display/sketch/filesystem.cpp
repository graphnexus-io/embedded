#include "filesystem.h"

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>

#include "config.h"

namespace {

bool card_ready = false;
bool current_mount_uses_fallback = false;
bool last_init_attempted_fallback = false;
char working_directory[FS_MAX_PATH + 1U] = "/";

bool card_has_io_error()
{
    Sd2Card *card = SdVolume::sdCard();
    return card != nullptr && card->errorCode() != 0U;
}

bool is_illegal_short_name_character(char character)
{
    switch (character) {
        case '|':
        case '<':
        case '>':
        case '^':
        case '+':
        case '=':
        case '?':
        case '/':
        case '[':
        case ']':
        case ';':
        case ',':
        case '*':
        case '"':
        case '\\':
        case ':':
            return true;
        default:
            return false;
    }
}

bool valid_short_name_component(const char *component, size_t length)
{
    if (component == nullptr || length == 0U || length > 12U) {
        return false;
    }
    if ((length == 1U && component[0] == '.') ||
        (length == 2U && component[0] == '.' && component[1] == '.')) {
        return false;
    }

    int8_t dot_position = -1;
    for (size_t index = 0U; index < length; ++index) {
        const uint8_t character = static_cast<uint8_t>(component[index]);
        if (character == static_cast<uint8_t>('.')) {
            if (dot_position >= 0) {
                return false;
            }
            dot_position = static_cast<int8_t>(index);
            continue;
        }
        if (character < 0x21U || character > 0x7EU ||
            is_illegal_short_name_character(static_cast<char>(character))) {
            return false;
        }
    }

    const size_t base_length = dot_position < 0 ? length : static_cast<size_t>(dot_position);
    const size_t extension_length = dot_position < 0 ?
        0U : length - static_cast<size_t>(dot_position) - 1U;
    return base_length >= 1U && base_length <= 8U && extension_length <= 3U &&
           (dot_position < 0 || extension_length >= 1U);
}

bool initialize_card()
{
    filesystem_prepare_access();
    SD.end();
    current_mount_uses_fallback = false;
    last_init_attempted_fallback = false;

    if (SD.begin(SD_CS_PIN)) {
        card_ready = true;
        filesystem_finish_access();
        return true;
    }

    filesystem_prepare_access();
    SD.end();
    last_init_attempted_fallback = true;
    if (SD.begin(SD_FALLBACK_SPI_HZ, SD_CS_PIN)) {
        card_ready = true;
        current_mount_uses_fallback = true;
        filesystem_finish_access();
        return true;
    }

    SD.end();
    card_ready = false;
    filesystem_finish_access();
    return false;
}

bool probe_root_directory()
{
    filesystem_prepare_access();
    File root = SD.open("/");
    bool valid = root && root.isDirectory();
    if (valid) {
        // Opening root can use cached volume metadata. Reading one entry (or
        // the end marker of an empty root) forces actual media I/O so a card
        // removed after the previous command is not mistaken for an empty FS.
        File probe = root.openNextFile();
        probe.close();
        valid = !card_has_io_error();
    }
    root.close();
    filesystem_finish_access();
    return valid;
}

FilesystemStatus inspect_path(
    const char *normalized_path, bool *exists, bool *is_directory, uint32_t *size)
{
    if (exists != nullptr) {
        *exists = false;
    }
    if (is_directory != nullptr) {
        *is_directory = false;
    }
    if (size != nullptr) {
        *size = 0UL;
    }

    filesystem_prepare_access();
    File target = SD.open(normalized_path, FILE_READ);
    if (!target) {
        target.close();
        filesystem_prepare_access();
        if (SD.exists(normalized_path)) {
            return FILESYSTEM_OPEN_FAILED;
        }
        if (card_has_io_error()) {
            return FILESYSTEM_NOT_READY;
        }
        return FILESYSTEM_NOT_FOUND;
    }

    if (exists != nullptr) {
        *exists = true;
    }
    const bool directory = target.isDirectory();
    if (is_directory != nullptr) {
        *is_directory = directory;
    }
    if (size != nullptr && !directory) {
        *size = target.size();
    }
    target.close();
    return FILESYSTEM_OK;
}

void make_parent_path(const char *normalized_path, char *parent)
{
    const char *last_separator = strrchr(normalized_path, '/');
    if (last_separator == normalized_path) {
        parent[0] = '/';
        parent[1] = '\0';
        return;
    }

    const size_t length = static_cast<size_t>(last_separator - normalized_path);
    memcpy(parent, normalized_path, length);
    parent[length] = '\0';
}

FilesystemStatus inspect_parent(const char *normalized_path)
{
    char parent[FS_MAX_PATH + 1U];
    make_parent_path(normalized_path, parent);

    bool exists = false;
    bool is_directory = false;
    const FilesystemStatus status = inspect_path(parent, &exists, &is_directory, nullptr);
    if (status == FILESYSTEM_NOT_FOUND || !exists) {
        return FILESYSTEM_PARENT_NOT_FOUND;
    }
    if (status != FILESYSTEM_OK) {
        return status;
    }
    return is_directory ? FILESYSTEM_OK : FILESYSTEM_PARENT_NOT_DIRECTORY;
}

void emit_character(
    char character,
    FilesystemCharacterWriter writer,
    void *context,
    bool *ended_with_newline)
{
    if (writer != nullptr) {
        writer(character, context);
    }
    *ended_with_newline = character == '\n';
}

bool write_file_bytes(File &file, const char *text, size_t length, uint32_t *count)
{
    if (length == 0U) {
        return true;
    }
    const size_t written = file.write(
        reinterpret_cast<const uint8_t *>(text), length);
    if (written != length) {
        return false;
    }
    *count += static_cast<uint32_t>(written);
    return true;
}

void pop_path_component(char *path, size_t *length)
{
    if (*length <= 1U) {
        return;
    }

    while (*length > 1U && path[*length - 1U] != '/') {
        --(*length);
    }
    if (*length > 1U) {
        --(*length);
    }
    path[*length] = '\0';
}

bool remove_partial_file(const char *normalized_path)
{
    filesystem_prepare_access();
    if (!SD.remove(normalized_path)) {
        filesystem_finish_access();
        return false;
    }
    filesystem_prepare_access();
    const bool removed = !SD.exists(normalized_path);
    filesystem_finish_access();
    return removed;
}

}  // namespace

FilesystemStatus filesystem_normalize_path(
    const char *input, char *normalized, size_t normalized_capacity)
{
    if (normalized == nullptr || normalized_capacity < 2U) {
        return FILESYSTEM_INVALID_PATH;
    }
    normalized[0] = '\0';
    if (input == nullptr || input[0] == '\0') {
        return FILESYSTEM_INVALID_PATH;
    }

    size_t raw_length = 0U;
    while (input[raw_length] != '\0') {
        const uint8_t character = static_cast<uint8_t>(input[raw_length]);
        if (raw_length >= FS_MAX_PATH) {
            return FILESYSTEM_PATH_TOO_LONG;
        }
        if (character < 0x20U || character == 0x7FU) {
            return FILESYSTEM_INVALID_PATH;
        }
        ++raw_length;
    }

    size_t output_length = 0U;
    const char *cursor = input;
    if (*cursor == '/') {
        normalized[output_length++] = '/';
        normalized[output_length] = '\0';
        while (*cursor == '/') {
            ++cursor;
        }
    } else {
        output_length = strlen(working_directory);
        if (output_length >= normalized_capacity) {
            return FILESYSTEM_PATH_TOO_LONG;
        }
        memcpy(normalized, working_directory, output_length + 1U);
    }

    while (*cursor != '\0') {
        const char *component = cursor;
        size_t component_length = 0U;
        while (cursor[component_length] != '\0' && cursor[component_length] != '/') {
            ++component_length;
        }
        const bool current_component =
            component_length == 1U && component[0] == '.';
        const bool parent_component =
            component_length == 2U && component[0] == '.' && component[1] == '.';
        if (current_component) {
            cursor += component_length;
            while (*cursor == '/') {
                ++cursor;
            }
            continue;
        }
        if (parent_component) {
            pop_path_component(normalized, &output_length);
            cursor += component_length;
            while (*cursor == '/') {
                ++cursor;
            }
            continue;
        }
        if (!valid_short_name_component(component, component_length)) {
            normalized[0] = '\0';
            return FILESYSTEM_INVALID_PATH;
        }

        if (output_length > 1U) {
            if (output_length >= FS_MAX_PATH || output_length >= normalized_capacity - 1U) {
                normalized[0] = '\0';
                return FILESYSTEM_PATH_TOO_LONG;
            }
            normalized[output_length++] = '/';
        }
        if (component_length > FS_MAX_PATH - output_length ||
            component_length >= normalized_capacity - output_length) {
            normalized[0] = '\0';
            return FILESYSTEM_PATH_TOO_LONG;
        }
        memcpy(&normalized[output_length], component, component_length);
        output_length += component_length;
        normalized[output_length] = '\0';

        cursor += component_length;
        while (*cursor == '/') {
            ++cursor;
        }
    }

    normalized[output_length] = '\0';
    return FILESYSTEM_OK;
}

const char *filesystem_working_directory()
{
    return working_directory;
}

FilesystemStatus filesystem_change_directory(const char *normalized_path)
{
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }

    bool exists = false;
    bool is_directory = false;
    const FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, nullptr);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (!is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_NOT_DIRECTORY;
    }

    const size_t length = strlen(normalized_path);
    memcpy(working_directory, normalized_path, length + 1U);
    filesystem_finish_access();
    return FILESYSTEM_OK;
}

bool filesystem_ensure_ready()
{
    if (card_ready && probe_root_directory()) {
        return true;
    }

    // A failed probe is the one controlled hot-removal recovery attempt for
    // this command. Standard SD does not expose richer media-change status.
    if (card_ready) {
        filesystem_end_session();
    }
    return initialize_card();
}

bool filesystem_last_init_used_fallback()
{
    return current_mount_uses_fallback;
}

bool filesystem_last_init_tried_fallback()
{
    return last_init_attempted_fallback;
}

void filesystem_prepare_access()
{
    // SD owns SD_CS while transferring. Starting with both devices idle keeps
    // the LCD from driving MISO during the SD library's SPI transaction.
    digitalWrite(LCD_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
}

void filesystem_finish_access()
{
    digitalWrite(SD_CS_PIN, HIGH);
    digitalWrite(LCD_CS_PIN, HIGH);
}

void filesystem_end_session()
{
    filesystem_prepare_access();
    SD.end();
    card_ready = false;
    current_mount_uses_fallback = false;
    last_init_attempted_fallback = false;
    filesystem_finish_access();
}

FilesystemStatus filesystem_list(
    const char *normalized_path,
    FilesystemEntryVisitor visitor,
    void *context,
    bool *was_empty)
{
    if (was_empty != nullptr) {
        *was_empty = true;
    }
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }

    bool exists = false;
    bool is_directory = false;
    FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, nullptr);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (!is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_NOT_DIRECTORY;
    }

    filesystem_prepare_access();
    File directory = SD.open(normalized_path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        filesystem_finish_access();
        return FILESYSTEM_OPEN_FAILED;
    }

    bool empty = true;
    while (true) {
        filesystem_prepare_access();
        File entry_file = directory.openNextFile();
        if (!entry_file) {
            entry_file.close();
            if (card_has_io_error()) {
                directory.close();
                filesystem_end_session();
                return FILESYSTEM_READ_FAILED;
            }
            break;
        }

        FilesystemEntry entry;
        strncpy(entry.name, entry_file.name(), sizeof(entry.name) - 1U);
        entry.name[sizeof(entry.name) - 1U] = '\0';
        entry.is_directory = entry_file.isDirectory();
        entry.size = entry.is_directory ? 0UL : entry_file.size();
        entry_file.close();
        filesystem_finish_access();

        empty = false;
        if (visitor != nullptr) {
            visitor(entry, context);
        }
    }
    directory.close();
    filesystem_finish_access();

    if (was_empty != nullptr) {
        *was_empty = empty;
    }
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_read_text(
    const char *normalized_path,
    uint32_t maximum_bytes,
    FilesystemCharacterWriter writer,
    void *context,
    FilesystemReadResult *result)
{
    if (result != nullptr) {
        result->truncated = false;
        result->ended_with_newline = false;
    }
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }

    bool exists = false;
    bool is_directory = false;
    uint32_t file_size = 0UL;
    FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, &file_size);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    filesystem_prepare_access();
    File file = SD.open(normalized_path, FILE_READ);
    if (!file || file.isDirectory()) {
        file.close();
        filesystem_finish_access();
        return FILESYSTEM_OPEN_FAILED;
    }

    const uint32_t bytes_to_read = file_size < maximum_bytes ? file_size : maximum_bytes;
    uint32_t bytes_read = 0UL;
    uint8_t buffer[FS_IO_BUFFER_SIZE];
    bool pending_carriage_return = false;
    bool ended_with_newline = false;

    while (bytes_read < bytes_to_read) {
        uint32_t remaining = bytes_to_read - bytes_read;
        const uint16_t request = static_cast<uint16_t>(
            remaining < sizeof(buffer) ? remaining : sizeof(buffer));
        filesystem_prepare_access();
        const int received = file.read(buffer, request);
        if (received <= 0) {
            file.close();
            if (card_has_io_error()) {
                filesystem_end_session();
            } else {
                filesystem_finish_access();
            }
            return FILESYSTEM_READ_FAILED;
        }
        bytes_read += static_cast<uint32_t>(received);
        filesystem_finish_access();

        for (int index = 0; index < received; ++index) {
            const uint8_t character = buffer[index];
            if (character == static_cast<uint8_t>('\r')) {
                if (pending_carriage_return) {
                    emit_character('\n', writer, context, &ended_with_newline);
                }
                pending_carriage_return = true;
                continue;
            }
            if (character == static_cast<uint8_t>('\n')) {
                pending_carriage_return = false;
                emit_character('\n', writer, context, &ended_with_newline);
                continue;
            }
            if (pending_carriage_return) {
                emit_character('\n', writer, context, &ended_with_newline);
                pending_carriage_return = false;
            }
            const char rendered = (character >= 0x20U && character <= 0x7EU) ?
                static_cast<char>(character) : '.';
            emit_character(rendered, writer, context, &ended_with_newline);
        }
    }

    filesystem_prepare_access();
    file.close();
    filesystem_finish_access();
    if (pending_carriage_return) {
        emit_character('\n', writer, context, &ended_with_newline);
    }

    if (result != nullptr) {
        result->truncated = file_size > maximum_bytes;
        result->ended_with_newline = ended_with_newline;
    }
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_write_text_line(
    const char *normalized_path,
    char *const text_parts[],
    uint8_t part_count,
    bool append,
    uint32_t *bytes_written)
{
    if (bytes_written != nullptr) {
        *bytes_written = 0UL;
    }
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }
    if (strcmp(normalized_path, "/") == 0) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    FilesystemStatus status = inspect_parent(normalized_path);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }

    bool exists = false;
    bool is_directory = false;
    uint32_t original_size = 0UL;
    status = inspect_path(normalized_path, &exists, &is_directory, &original_size);
    if (status != FILESYSTEM_OK && status != FILESYSTEM_NOT_FOUND) {
        filesystem_finish_access();
        return status;
    }
    if (exists && is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    if (!append && exists) {
        filesystem_prepare_access();
        if (!SD.remove(normalized_path)) {
            filesystem_finish_access();
            return FILESYSTEM_REMOVE_FAILED;
        }
        filesystem_prepare_access();
        if (SD.exists(normalized_path)) {
            filesystem_finish_access();
            return FILESYSTEM_VERIFY_FAILED;
        }
        original_size = 0UL;
    }

    filesystem_prepare_access();
    File file = SD.open(normalized_path, FILE_WRITE);
    if (!file || file.isDirectory()) {
        file.close();
        filesystem_finish_access();
        return FILESYSTEM_OPEN_FAILED;
    }

    uint32_t count = 0UL;
    bool write_ok = true;
    for (uint8_t index = 0U; index < part_count && write_ok; ++index) {
        if (index > 0U) {
            write_ok = write_file_bytes(file, " ", 1U, &count);
        }
        if (write_ok) {
            if (text_parts == nullptr || text_parts[index] == nullptr) {
                write_ok = false;
            } else {
                write_ok = write_file_bytes(
                    file, text_parts[index], strlen(text_parts[index]), &count);
            }
        }
    }
    if (write_ok) {
        write_ok = write_file_bytes(file, "\n", 1U, &count);
    }
    file.flush();
    write_ok = write_ok && file.getWriteError() == 0;
    file.close();
    if (!write_ok) {
        filesystem_finish_access();
        return FILESYSTEM_WRITE_FAILED;
    }

    if (append && UINT32_MAX - original_size < count) {
        filesystem_finish_access();
        return FILESYSTEM_VERIFY_FAILED;
    }
    const uint32_t expected_size = append ? original_size + count : count;
    filesystem_prepare_access();
    File verification = SD.open(normalized_path, FILE_READ);
    if (!verification || verification.isDirectory() || verification.size() != expected_size) {
        verification.close();
        filesystem_finish_access();
        return FILESYSTEM_VERIFY_FAILED;
    }
    verification.close();
    filesystem_finish_access();

    if (bytes_written != nullptr) {
        *bytes_written = count;
    }
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_open_text_for_edit(
    const char *normalized_path,
    char *buffer,
    uint16_t buffer_capacity,
    uint16_t *length,
    bool *is_new_file)
{
    if (buffer == nullptr || buffer_capacity == 0U ||
        length == nullptr || is_new_file == nullptr) {
        return FILESYSTEM_INVALID_PATH;
    }
    buffer[0] = '\0';
    *length = 0U;
    *is_new_file = false;

    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }
    if (strcmp(normalized_path, "/") == 0) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    bool exists = false;
    bool is_directory = false;
    uint32_t file_size = 0UL;
    FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, &file_size);
    if (status == FILESYSTEM_NOT_FOUND) {
        status = inspect_parent(normalized_path);
        filesystem_finish_access();
        if (status == FILESYSTEM_OK) {
            *is_new_file = true;
        }
        return status;
    }
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }
    if (file_size >= buffer_capacity) {
        filesystem_finish_access();
        return FILESYSTEM_FILE_TOO_LARGE;
    }

    filesystem_prepare_access();
    File file = SD.open(normalized_path, FILE_READ);
    if (!file || file.isDirectory()) {
        file.close();
        filesystem_finish_access();
        return FILESYSTEM_OPEN_FAILED;
    }

    uint8_t io_buffer[FS_IO_BUFFER_SIZE];
    uint32_t raw_bytes_read = 0UL;
    uint16_t text_length = 0U;
    bool pending_carriage_return = false;
    while (raw_bytes_read < file_size) {
        const uint32_t remaining = file_size - raw_bytes_read;
        const uint16_t request = static_cast<uint16_t>(
            remaining < sizeof(io_buffer) ? remaining : sizeof(io_buffer));
        filesystem_prepare_access();
        const int received = file.read(io_buffer, request);
        if (received != static_cast<int>(request)) {
            file.close();
            if (card_has_io_error()) {
                filesystem_end_session();
            } else {
                filesystem_finish_access();
            }
            buffer[0] = '\0';
            return FILESYSTEM_READ_FAILED;
        }
        raw_bytes_read += static_cast<uint32_t>(received);

        for (int index = 0; index < received; ++index) {
            const uint8_t character = io_buffer[index];
            if (character == static_cast<uint8_t>('\r')) {
                if (pending_carriage_return) {
                    buffer[text_length++] = '\n';
                }
                pending_carriage_return = true;
                continue;
            }
            if (character == static_cast<uint8_t>('\n')) {
                buffer[text_length++] = '\n';
                pending_carriage_return = false;
                continue;
            }
            if (pending_carriage_return) {
                buffer[text_length++] = '\n';
                pending_carriage_return = false;
            }
            if (character < 0x20U || character > 0x7EU) {
                file.close();
                filesystem_finish_access();
                buffer[0] = '\0';
                return FILESYSTEM_NOT_TEXT;
            }
            buffer[text_length++] = static_cast<char>(character);
        }
    }
    file.close();
    filesystem_finish_access();
    if (pending_carriage_return) {
        buffer[text_length++] = '\n';
    }
    buffer[text_length] = '\0';
    *length = text_length;
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_write_text_file(
    const char *normalized_path, const char *text, uint16_t length)
{
    if (text == nullptr) {
        return FILESYSTEM_WRITE_FAILED;
    }
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }
    if (strcmp(normalized_path, "/") == 0) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    FilesystemStatus status = inspect_parent(normalized_path);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }

    bool exists = false;
    bool is_directory = false;
    status = inspect_path(normalized_path, &exists, &is_directory, nullptr);
    if (status != FILESYSTEM_OK && status != FILESYSTEM_NOT_FOUND) {
        filesystem_finish_access();
        return status;
    }
    if (exists && is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    if (exists) {
        filesystem_prepare_access();
        if (!SD.remove(normalized_path)) {
            filesystem_finish_access();
            return FILESYSTEM_REMOVE_FAILED;
        }
        filesystem_prepare_access();
        if (SD.exists(normalized_path)) {
            filesystem_finish_access();
            return FILESYSTEM_VERIFY_FAILED;
        }
    }

    filesystem_prepare_access();
    File file = SD.open(normalized_path, FILE_WRITE);
    if (!file || file.isDirectory()) {
        file.close();
        filesystem_finish_access();
        return FILESYSTEM_OPEN_FAILED;
    }

    uint16_t written_total = 0U;
    bool write_ok = true;
    while (written_total < length && write_ok) {
        const uint16_t remaining = static_cast<uint16_t>(length - written_total);
        const uint16_t request = static_cast<uint16_t>(
            remaining < FS_IO_BUFFER_SIZE ? remaining : FS_IO_BUFFER_SIZE);
        filesystem_prepare_access();
        const size_t written = file.write(
            reinterpret_cast<const uint8_t *>(&text[written_total]), request);
        if (written != request) {
            write_ok = false;
        } else {
            written_total = static_cast<uint16_t>(written_total + written);
        }
    }
    file.flush();
    write_ok = write_ok && file.getWriteError() == 0;
    file.close();
    filesystem_finish_access();
    if (!write_ok || written_total != length) {
        return FILESYSTEM_WRITE_FAILED;
    }

    filesystem_prepare_access();
    File verification = SD.open(normalized_path, FILE_READ);
    bool verified = verification && !verification.isDirectory() &&
        verification.size() == static_cast<uint32_t>(length);
    uint8_t verify_buffer[FS_IO_BUFFER_SIZE];
    uint16_t verified_total = 0U;
    while (verified && verified_total < length) {
        const uint16_t remaining = static_cast<uint16_t>(length - verified_total);
        const uint16_t request = static_cast<uint16_t>(
            remaining < sizeof(verify_buffer) ? remaining : sizeof(verify_buffer));
        filesystem_prepare_access();
        const int received = verification.read(verify_buffer, request);
        if (received != static_cast<int>(request) ||
            memcmp(verify_buffer, &text[verified_total], request) != 0) {
            verified = false;
            break;
        }
        verified_total = static_cast<uint16_t>(verified_total + request);
    }
    verification.close();
    filesystem_finish_access();
    return verified && verified_total == length ?
        FILESYSTEM_OK : FILESYSTEM_VERIFY_FAILED;
}

FilesystemStatus filesystem_make_directory(const char *normalized_path)
{
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }
    if (strcmp(normalized_path, "/") == 0) {
        filesystem_finish_access();
        return FILESYSTEM_ROOT_DENIED;
    }

    bool exists = false;
    bool is_directory = false;
    FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, nullptr);
    if (status == FILESYSTEM_OK && exists) {
        filesystem_finish_access();
        return is_directory ? FILESYSTEM_EXISTS_AS_DIRECTORY : FILESYSTEM_EXISTS_AS_FILE;
    }
    if (status != FILESYSTEM_NOT_FOUND) {
        filesystem_finish_access();
        return status;
    }

    status = inspect_parent(normalized_path);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }

    filesystem_prepare_access();
    if (!SD.mkdir(normalized_path)) {
        filesystem_finish_access();
        return FILESYSTEM_CREATE_FAILED;
    }

    status = inspect_path(normalized_path, &exists, &is_directory, nullptr);
    filesystem_finish_access();
    if (status != FILESYSTEM_OK || !exists || !is_directory) {
        return FILESYSTEM_VERIFY_FAILED;
    }
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_remove_directory(const char *normalized_path)
{
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }
    if (strcmp(normalized_path, "/") == 0) {
        filesystem_finish_access();
        return FILESYSTEM_ROOT_DENIED;
    }

    bool exists = false;
    bool is_directory = false;
    FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, nullptr);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (!is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_NOT_DIRECTORY;
    }

    filesystem_prepare_access();
    File directory = SD.open(normalized_path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        filesystem_finish_access();
        return FILESYSTEM_OPEN_FAILED;
    }
    filesystem_prepare_access();
    File first_entry = directory.openNextFile();
    const bool empty = !first_entry;
    first_entry.close();
    directory.close();
    if (card_has_io_error()) {
        filesystem_end_session();
        return FILESYSTEM_READ_FAILED;
    }
    if (!empty) {
        filesystem_finish_access();
        return FILESYSTEM_NOT_EMPTY;
    }

    filesystem_prepare_access();
    if (!SD.rmdir(normalized_path)) {
        filesystem_finish_access();
        return FILESYSTEM_REMOVE_FAILED;
    }
    filesystem_prepare_access();
    if (SD.exists(normalized_path)) {
        filesystem_finish_access();
        return FILESYSTEM_VERIFY_FAILED;
    }
    filesystem_finish_access();
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_remove_file(const char *normalized_path)
{
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }

    bool exists = false;
    bool is_directory = false;
    FilesystemStatus status = inspect_path(
        normalized_path, &exists, &is_directory, nullptr);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    filesystem_prepare_access();
    if (!SD.remove(normalized_path)) {
        filesystem_finish_access();
        return FILESYSTEM_REMOVE_FAILED;
    }
    filesystem_prepare_access();
    if (SD.exists(normalized_path)) {
        filesystem_finish_access();
        return FILESYSTEM_VERIFY_FAILED;
    }
    filesystem_finish_access();
    return FILESYSTEM_OK;
}

FilesystemStatus filesystem_move_file(
    const char *normalized_source, const char *normalized_destination)
{
    if (strcmp(normalized_source, normalized_destination) == 0) {
        return FILESYSTEM_SAME_PATH;
    }
    if (!filesystem_ensure_ready()) {
        return FILESYSTEM_NOT_READY;
    }

    bool source_exists = false;
    bool source_is_directory = false;
    uint32_t source_size = 0UL;
    FilesystemStatus status = inspect_path(
        normalized_source, &source_exists, &source_is_directory, &source_size);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }
    if (source_is_directory) {
        filesystem_finish_access();
        return FILESYSTEM_IS_DIRECTORY;
    }

    bool destination_exists = false;
    bool destination_is_directory = false;
    status = inspect_path(
        normalized_destination, &destination_exists, &destination_is_directory, nullptr);
    if (status == FILESYSTEM_OK && destination_exists) {
        filesystem_finish_access();
        return FILESYSTEM_DESTINATION_EXISTS;
    }
    if (status != FILESYSTEM_NOT_FOUND) {
        filesystem_finish_access();
        return status;
    }

    status = inspect_parent(normalized_destination);
    if (status != FILESYSTEM_OK) {
        filesystem_finish_access();
        return status;
    }

    filesystem_prepare_access();
    File source = SD.open(normalized_source, FILE_READ);
    File destination = SD.open(normalized_destination, FILE_WRITE);
    if (!source || source.isDirectory() || !destination || destination.isDirectory()) {
        source.close();
        destination.close();
        const bool destination_created = SD.exists(normalized_destination);
        filesystem_finish_access();
        if (destination_created && !remove_partial_file(normalized_destination)) {
            return FILESYSTEM_CLEANUP_FAILED;
        }
        return FILESYSTEM_OPEN_FAILED;
    }

    uint8_t copy_buffer[FS_IO_BUFFER_SIZE];
    uint32_t copied = 0UL;
    bool copy_ok = true;
    while (copied < source_size && copy_ok) {
        const uint32_t remaining = source_size - copied;
        const uint16_t request = static_cast<uint16_t>(
            remaining < sizeof(copy_buffer) ? remaining : sizeof(copy_buffer));
        filesystem_prepare_access();
        const int received = source.read(copy_buffer, request);
        if (received != static_cast<int>(request)) {
            copy_ok = false;
            break;
        }
        filesystem_prepare_access();
        const size_t written = destination.write(copy_buffer, request);
        if (written != request) {
            copy_ok = false;
            break;
        }
        copied += static_cast<uint32_t>(written);
    }
    destination.flush();
    copy_ok = copy_ok && destination.getWriteError() == 0;
    source.close();
    destination.close();
    filesystem_finish_access();
    if (!copy_ok || copied != source_size) {
        return remove_partial_file(normalized_destination) ?
            FILESYSTEM_COPY_FAILED : FILESYSTEM_CLEANUP_FAILED;
    }

    filesystem_prepare_access();
    File verify_source = SD.open(normalized_source, FILE_READ);
    File verify_destination = SD.open(normalized_destination, FILE_READ);
    bool verification_ok = verify_source && !verify_source.isDirectory() &&
        verify_destination && !verify_destination.isDirectory() &&
        verify_source.size() == source_size && verify_destination.size() == source_size;
    uint8_t source_buffer[FS_IO_BUFFER_SIZE];
    uint8_t destination_buffer[FS_IO_BUFFER_SIZE];
    uint32_t verified = 0UL;
    while (verification_ok && verified < source_size) {
        const uint32_t remaining = source_size - verified;
        const uint16_t request = static_cast<uint16_t>(
            remaining < sizeof(source_buffer) ? remaining : sizeof(source_buffer));
        filesystem_prepare_access();
        const int source_count = verify_source.read(source_buffer, request);
        filesystem_prepare_access();
        const int destination_count = verify_destination.read(destination_buffer, request);
        if (source_count != static_cast<int>(request) ||
            destination_count != static_cast<int>(request) ||
            memcmp(source_buffer, destination_buffer, request) != 0) {
            verification_ok = false;
            break;
        }
        verified += request;
    }
    verify_source.close();
    verify_destination.close();
    filesystem_finish_access();
    if (!verification_ok || verified != source_size) {
        return remove_partial_file(normalized_destination) ?
            FILESYSTEM_VERIFY_FAILED : FILESYSTEM_CLEANUP_FAILED;
    }

    filesystem_prepare_access();
    if (!SD.remove(normalized_source)) {
        filesystem_finish_access();
        return FILESYSTEM_SOURCE_REMOVE_FAILED;
    }
    filesystem_prepare_access();
    if (SD.exists(normalized_source)) {
        filesystem_finish_access();
        return FILESYSTEM_SOURCE_REMOVE_FAILED;
    }
    filesystem_finish_access();
    return FILESYSTEM_OK;
}
