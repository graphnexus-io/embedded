#include "commands_filesystem.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "console.h"
#include "filesystem.h"
#include "text_editor.h"

namespace {

void print_usage(const __FlashStringHelper *usage)
{
    console_print(F("Usage: "));
    console_println(usage);
}

bool normalize_or_report(
    const char *input, char *normalized, CommandStatus *command_status)
{
    const FilesystemStatus status = filesystem_normalize_path(
        input, normalized, FS_MAX_PATH + 1U);
    if (status == FILESYSTEM_OK) {
        return true;
    }
    if (status == FILESYSTEM_PATH_TOO_LONG) {
        console_println(F("Error: path too long"));
    } else {
        console_println(F("Error: invalid path"));
    }
    *command_status = COMMAND_BAD_ARGUMENTS;
    return false;
}

CommandStatus report_sd_or_open_error(
    FilesystemStatus status,
    const __FlashStringHelper *not_found,
    const __FlashStringHelper *open_failed)
{
    if (status == FILESYSTEM_NOT_READY) {
        console_println(F("SD card is not ready"));
    } else if (status == FILESYSTEM_NOT_FOUND) {
        console_println(not_found);
    } else if (status == FILESYSTEM_OPEN_FAILED) {
        console_println(open_failed);
    } else {
        console_println(F("Error: filesystem operation failed"));
    }
    return COMMAND_FAILED;
}

void list_entry(const FilesystemEntry &entry, void *)
{
    console_print(entry.is_directory ? F("[DIR ] ") : F("[FILE] "));
    console_print(entry.name);
    if (!entry.is_directory) {
        console_print(F(" "));
        console_print_u32(entry.size);
        console_print(F(" bytes"));
    }
    console_println();
}

void write_cat_character(char character, void *)
{
    console_write(character);
}

CommandStatus report_echo_filesystem_error(FilesystemStatus status)
{
    switch (status) {
        case FILESYSTEM_NOT_READY:
            console_println(F("SD card is not ready"));
            break;
        case FILESYSTEM_PARENT_NOT_FOUND:
            console_println(F("Error: parent directory does not exist"));
            break;
        case FILESYSTEM_PARENT_NOT_DIRECTORY:
            console_println(F("Error: parent is not a directory"));
            break;
        case FILESYSTEM_IS_DIRECTORY:
            console_println(F("Error: target is a directory"));
            break;
        case FILESYSTEM_REMOVE_FAILED:
            console_println(F("Error: existing file removal failed"));
            break;
        case FILESYSTEM_OPEN_FAILED:
            console_println(F("Error: file open failed"));
            break;
        case FILESYSTEM_WRITE_FAILED:
            console_println(F("Error: file write failed"));
            break;
        case FILESYSTEM_VERIFY_FAILED:
            console_println(F("Error: file write verification failed"));
            break;
        default:
            console_println(F("Error: filesystem operation failed"));
            break;
    }
    return COMMAND_FAILED;
}

}  // namespace

CommandStatus command_fs_echo(int argc, char *argv[])
{
    int8_t redirect_index = -1;
    bool append = false;
    for (int index = 1; index < argc; ++index) {
        const bool overwrite_operator = strcmp(argv[index], ">") == 0;
        const bool append_operator = strcmp(argv[index], ">>") == 0;
        if (!overwrite_operator && !append_operator) {
            continue;
        }
        if (redirect_index >= 0) {
            console_println(F("Error: multiple redirections"));
            print_usage(F("echo [text...] [> file | >> file]"));
            return COMMAND_BAD_ARGUMENTS;
        }
        redirect_index = static_cast<int8_t>(index);
        append = append_operator;
    }

    if (redirect_index < 0) {
        // Preserve the original console-only echo behavior exactly.
        for (int index = 1; index < argc; ++index) {
            if (index > 1) {
                console_print(F(" "));
            }
            console_print(argv[index]);
        }
        console_println();
        return COMMAND_OK;
    }

    if (redirect_index + 1 >= argc) {
        console_println(F("Error: missing redirection filename"));
        print_usage(F("echo [text...] [> file | >> file]"));
        return COMMAND_BAD_ARGUMENTS;
    }
    if (redirect_index + 2 != argc) {
        console_println(F("Error: redirection filename must be the final argument"));
        print_usage(F("echo [text...] [> file | >> file]"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[redirect_index + 1], normalized, &command_status)) {
        return command_status;
    }

    uint32_t bytes_written = 0UL;
    const FilesystemStatus status = filesystem_write_text_line(
        normalized,
        &argv[1],
        static_cast<uint8_t>(redirect_index - 1),
        append,
        &bytes_written);
    if (status != FILESYSTEM_OK) {
        return report_echo_filesystem_error(status);
    }

    console_print(append ? F("Appended ") : F("Wrote "));
    console_print_u32(bytes_written);
    console_print(F(" bytes to "));
    console_println(normalized);
    return COMMAND_OK;
}

CommandStatus command_fs_pwd(int argc, char *[])
{
    if (argc != 1) {
        console_println(F("Error: too many arguments"));
        print_usage(F("pwd"));
        return COMMAND_BAD_ARGUMENTS;
    }

    console_println(filesystem_working_directory());
    return COMMAND_OK;
}

CommandStatus command_fs_cd(int argc, char *argv[])
{
    if (argc > 2) {
        console_println(F("Error: too many arguments"));
        print_usage(F("cd [directory]"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    const char *requested = argc == 1 ? "/" : argv[1];
    if (!normalize_or_report(requested, normalized, &command_status)) {
        return command_status;
    }

    const FilesystemStatus status = filesystem_change_directory(normalized);
    if (status == FILESYSTEM_OK) {
        // Like a traditional shell, cd is silent on success; pwd reports it.
        return COMMAND_OK;
    }
    if (status == FILESYSTEM_NOT_DIRECTORY) {
        console_println(F("Error: target is not a directory"));
        return COMMAND_FAILED;
    }
    return report_sd_or_open_error(
        status, F("Error: directory not found"), F("Error: directory open failed"));
}

CommandStatus command_fs_ls(int argc, char *argv[])
{
    if (argc > 2) {
        console_println(F("Error: too many arguments"));
        print_usage(F("ls [directory]"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    const char *requested = argc == 1 ? "." : argv[1];
    if (!normalize_or_report(requested, normalized, &command_status)) {
        return command_status;
    }

    bool empty = true;
    const FilesystemStatus status = filesystem_list(
        normalized, list_entry, nullptr, &empty);
    if (status == FILESYSTEM_NOT_DIRECTORY) {
        console_println(F("Error: target is not a directory"));
        return COMMAND_FAILED;
    }
    if (status == FILESYSTEM_READ_FAILED) {
        console_println(F("Error: directory read failed"));
        return COMMAND_FAILED;
    }
    if (status != FILESYSTEM_OK) {
        return report_sd_or_open_error(
            status, F("Error: directory not found"), F("Error: directory open failed"));
    }
    if (empty) {
        console_println(F("Directory is empty"));
    }
    return COMMAND_OK;
}

CommandStatus command_fs_cat(int argc, char *argv[])
{
    if (argc != 2) {
        console_println(argc < 2 ? F("Error: missing file path") : F("Error: too many arguments"));
        print_usage(F("cat <file>"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[1], normalized, &command_status)) {
        return command_status;
    }

    FilesystemReadResult result;
    const FilesystemStatus status = filesystem_read_text(
        normalized, CAT_MAX_BYTES, write_cat_character, nullptr, &result);
    if (status == FILESYSTEM_IS_DIRECTORY) {
        console_println(F("Error: target is a directory"));
        return COMMAND_FAILED;
    }
    if (status == FILESYSTEM_READ_FAILED) {
        console_println(F("Error: file read failed"));
        return COMMAND_FAILED;
    }
    if (status != FILESYSTEM_OK) {
        return report_sd_or_open_error(
            status, F("Error: file not found"), F("Error: file open failed"));
    }

    if (result.truncated) {
        if (!result.ended_with_newline) {
            console_println();
        }
        console_println(F("[output truncated]"));
    }
    return COMMAND_OK;
}

CommandStatus command_fs_mkdir(int argc, char *argv[])
{
    if (argc != 2) {
        console_println(
            argc < 2 ? F("Error: missing directory path") : F("Error: too many arguments"));
        print_usage(F("mkdir <directory>"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[1], normalized, &command_status)) {
        return command_status;
    }

    const FilesystemStatus status = filesystem_make_directory(normalized);
    switch (status) {
        case FILESYSTEM_OK:
            console_print(F("Directory created: "));
            console_println(normalized);
            return COMMAND_OK;
        case FILESYSTEM_ROOT_DENIED:
            console_println(F("Error: cannot create root directory"));
            return COMMAND_DENIED;
        case FILESYSTEM_EXISTS_AS_DIRECTORY:
            console_println(F("Error: directory already exists"));
            return COMMAND_FAILED;
        case FILESYSTEM_EXISTS_AS_FILE:
            console_println(F("Error: path already exists as a file"));
            return COMMAND_FAILED;
        case FILESYSTEM_PARENT_NOT_FOUND:
            console_println(F("Error: parent directory does not exist"));
            return COMMAND_FAILED;
        case FILESYSTEM_PARENT_NOT_DIRECTORY:
            console_println(F("Error: parent is not a directory"));
            return COMMAND_FAILED;
        case FILESYSTEM_CREATE_FAILED:
            console_println(F("Error: directory creation failed"));
            return COMMAND_FAILED;
        case FILESYSTEM_VERIFY_FAILED:
            console_println(F("Error: directory creation verification failed"));
            return COMMAND_FAILED;
        default:
            return report_sd_or_open_error(
                status, F("Error: parent directory does not exist"),
                F("Error: directory open failed"));
    }
}

CommandStatus command_fs_rmdir(int argc, char *argv[])
{
    if (argc != 2) {
        console_println(
            argc < 2 ? F("Error: missing directory path") : F("Error: too many arguments"));
        print_usage(F("rmdir <directory>"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[1], normalized, &command_status)) {
        return command_status;
    }

    const FilesystemStatus status = filesystem_remove_directory(normalized);
    switch (status) {
        case FILESYSTEM_OK:
            console_print(F("Directory removed: "));
            console_println(normalized);
            return COMMAND_OK;
        case FILESYSTEM_ROOT_DENIED:
            console_println(F("Error: cannot remove root directory"));
            return COMMAND_DENIED;
        case FILESYSTEM_NOT_FOUND:
            console_println(F("Error: directory not found"));
            return COMMAND_FAILED;
        case FILESYSTEM_NOT_DIRECTORY:
            console_println(F("Error: target is not a directory"));
            return COMMAND_FAILED;
        case FILESYSTEM_NOT_EMPTY:
            console_println(F("Error: directory is not empty"));
            return COMMAND_DENIED;
        case FILESYSTEM_READ_FAILED:
            console_println(F("Error: directory read failed"));
            return COMMAND_FAILED;
        case FILESYSTEM_REMOVE_FAILED:
            console_println(F("Error: directory removal failed"));
            return COMMAND_FAILED;
        case FILESYSTEM_VERIFY_FAILED:
            console_println(F("Error: directory removal verification failed"));
            return COMMAND_FAILED;
        default:
            return report_sd_or_open_error(
                status, F("Error: directory not found"), F("Error: directory open failed"));
    }
}

CommandStatus command_fs_rm(int argc, char *argv[])
{
    if (argc != 2) {
        console_println(argc < 2 ? F("Error: missing file path") : F("Error: too many arguments"));
        print_usage(F("rm <file>"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[1], normalized, &command_status)) {
        return command_status;
    }

    const FilesystemStatus status = filesystem_remove_file(normalized);
    switch (status) {
        case FILESYSTEM_OK:
            console_print(F("File removed: "));
            console_println(normalized);
            return COMMAND_OK;
        case FILESYSTEM_NOT_FOUND:
            console_println(F("Error: file not found"));
            return COMMAND_FAILED;
        case FILESYSTEM_IS_DIRECTORY:
            console_println(F("Error: target is a directory"));
            return COMMAND_DENIED;
        case FILESYSTEM_REMOVE_FAILED:
            console_println(F("Error: file removal failed"));
            return COMMAND_FAILED;
        case FILESYSTEM_VERIFY_FAILED:
            console_println(F("Error: file removal verification failed"));
            return COMMAND_FAILED;
        default:
            return report_sd_or_open_error(
                status, F("Error: file not found"), F("Error: file open failed"));
    }
}

CommandStatus command_fs_mv(int argc, char *argv[])
{
    if (argc != 3) {
        console_println(
            argc < 3 ? F("Error: source and destination are required") :
                       F("Error: too many arguments"));
        print_usage(F("mv <source> <destination>"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char source[FS_MAX_PATH + 1U];
    char destination[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[1], source, &command_status) ||
        !normalize_or_report(argv[2], destination, &command_status)) {
        return command_status;
    }

    const FilesystemStatus status = filesystem_move_file(source, destination);
    switch (status) {
        case FILESYSTEM_OK:
            console_print(F("Moved: "));
            console_print(source);
            console_print(F(" -> "));
            console_println(destination);
            return COMMAND_OK;
        case FILESYSTEM_SAME_PATH:
            console_println(F("Error: source and destination are the same"));
            return COMMAND_BAD_ARGUMENTS;
        case FILESYSTEM_NOT_FOUND:
            console_println(F("Error: source file not found"));
            return COMMAND_FAILED;
        case FILESYSTEM_IS_DIRECTORY:
            console_println(F("Error: directory moves are not supported"));
            return COMMAND_DENIED;
        case FILESYSTEM_DESTINATION_EXISTS:
            console_println(F("Error: destination already exists"));
            return COMMAND_DENIED;
        case FILESYSTEM_PARENT_NOT_FOUND:
            console_println(F("Error: destination parent does not exist"));
            return COMMAND_FAILED;
        case FILESYSTEM_PARENT_NOT_DIRECTORY:
            console_println(F("Error: destination parent is not a directory"));
            return COMMAND_FAILED;
        case FILESYSTEM_COPY_FAILED:
            console_println(F("Error: file copy failed; partial destination removed"));
            return COMMAND_FAILED;
        case FILESYSTEM_VERIFY_FAILED:
            console_println(F("Error: copied file verification failed; destination removed"));
            return COMMAND_FAILED;
        case FILESYSTEM_CLEANUP_FAILED:
            console_println(F("Error: move failed; partial destination could not be removed"));
            return COMMAND_FAILED;
        case FILESYSTEM_SOURCE_REMOVE_FAILED:
            console_println(F("Error: source removal failed; both files remain"));
            return COMMAND_FAILED;
        default:
            return report_sd_or_open_error(
                status, F("Error: source file not found"), F("Error: file open failed"));
    }
}

CommandStatus command_fs_nano(int argc, char *argv[])
{
    if (argc != 2) {
        console_println(argc < 2 ? F("Error: missing file path") : F("Error: too many arguments"));
        print_usage(F("nano <file>"));
        return COMMAND_BAD_ARGUMENTS;
    }

    char normalized[FS_MAX_PATH + 1U];
    CommandStatus command_status = COMMAND_OK;
    if (!normalize_or_report(argv[1], normalized, &command_status)) {
        return command_status;
    }

    const FilesystemStatus status = text_editor_open(normalized);
    switch (status) {
        case FILESYSTEM_OK:
            return COMMAND_OK;
        case FILESYSTEM_IS_DIRECTORY:
            console_println(F("Error: target is a directory"));
            return COMMAND_FAILED;
        case FILESYSTEM_FILE_TOO_LARGE:
            console_println(F("Error: file exceeds the 1024-byte editor limit"));
            return COMMAND_DENIED;
        case FILESYSTEM_NOT_TEXT:
            console_println(F("Error: file contains unsupported non-text bytes"));
            return COMMAND_DENIED;
        case FILESYSTEM_PARENT_NOT_FOUND:
            console_println(F("Error: parent directory does not exist"));
            return COMMAND_FAILED;
        case FILESYSTEM_PARENT_NOT_DIRECTORY:
            console_println(F("Error: parent is not a directory"));
            return COMMAND_FAILED;
        case FILESYSTEM_READ_FAILED:
            console_println(F("Error: file read failed"));
            return COMMAND_FAILED;
        default:
            return report_sd_or_open_error(
                status, F("Error: file not found"), F("Error: file open failed"));
    }
}
