#ifndef MINIOS_FILESYSTEM_H
#define MINIOS_FILESYSTEM_H

#include <Arduino.h>

enum FilesystemStatus : uint8_t {
    FILESYSTEM_OK = 0,
    FILESYSTEM_INVALID_PATH,
    FILESYSTEM_PATH_TOO_LONG,
    FILESYSTEM_NOT_READY,
    FILESYSTEM_NOT_FOUND,
    FILESYSTEM_NOT_DIRECTORY,
    FILESYSTEM_IS_DIRECTORY,
    FILESYSTEM_OPEN_FAILED,
    FILESYSTEM_READ_FAILED,
    FILESYSTEM_WRITE_FAILED,
    FILESYSTEM_PARENT_NOT_FOUND,
    FILESYSTEM_PARENT_NOT_DIRECTORY,
    FILESYSTEM_EXISTS_AS_FILE,
    FILESYSTEM_EXISTS_AS_DIRECTORY,
    FILESYSTEM_ROOT_DENIED,
    FILESYSTEM_NOT_EMPTY,
    FILESYSTEM_REMOVE_FAILED,
    FILESYSTEM_CREATE_FAILED,
    FILESYSTEM_VERIFY_FAILED,
    FILESYSTEM_DESTINATION_EXISTS,
    FILESYSTEM_SAME_PATH,
    FILESYSTEM_COPY_FAILED,
    FILESYSTEM_CLEANUP_FAILED,
    FILESYSTEM_SOURCE_REMOVE_FAILED,
    FILESYSTEM_FILE_TOO_LARGE,
    FILESYSTEM_NOT_TEXT,
};

struct FilesystemEntry {
    char name[13];
    bool is_directory;
    uint32_t size;
};

struct FilesystemReadResult {
    bool truncated;
    bool ended_with_newline;
};

using FilesystemEntryVisitor = void (*)(const FilesystemEntry &entry, void *context);
using FilesystemCharacterWriter = void (*)(char character, void *context);

FilesystemStatus filesystem_normalize_path(
    const char *input, char *normalized, size_t normalized_capacity);
const char *filesystem_working_directory();
FilesystemStatus filesystem_change_directory(const char *normalized_path);

// Readiness and bus helpers are shared with the existing SD diagnostics.
bool filesystem_ensure_ready();
bool filesystem_last_init_used_fallback();
bool filesystem_last_init_tried_fallback();
void filesystem_prepare_access();
void filesystem_finish_access();
void filesystem_end_session();

FilesystemStatus filesystem_list(
    const char *normalized_path,
    FilesystemEntryVisitor visitor,
    void *context,
    bool *was_empty);
FilesystemStatus filesystem_read_text(
    const char *normalized_path,
    uint32_t maximum_bytes,
    FilesystemCharacterWriter writer,
    void *context,
    FilesystemReadResult *result);
FilesystemStatus filesystem_write_text_line(
    const char *normalized_path,
    char *const text_parts[],
    uint8_t part_count,
    bool append,
    uint32_t *bytes_written);
FilesystemStatus filesystem_open_text_for_edit(
    const char *normalized_path,
    char *buffer,
    uint16_t buffer_capacity,
    uint16_t *length,
    bool *is_new_file);
FilesystemStatus filesystem_write_text_file(
    const char *normalized_path, const char *text, uint16_t length);
FilesystemStatus filesystem_make_directory(const char *normalized_path);
FilesystemStatus filesystem_remove_directory(const char *normalized_path);
FilesystemStatus filesystem_remove_file(const char *normalized_path);
FilesystemStatus filesystem_move_file(
    const char *normalized_source, const char *normalized_destination);

#endif
