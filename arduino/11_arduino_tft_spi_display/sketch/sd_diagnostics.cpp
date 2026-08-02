#include "sd_diagnostics.h"

#include <Arduino.h>
#include <SD.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "config.h"
#include "console.h"
#include "filesystem.h"
#include "tft_terminal.h"

namespace {

constexpr char TEST_FILE_PATH[] = "/SDTEST.TXT";

const char TEST_LINE_1[] PROGMEM = "MiniOS SD test line 1";
const char TEST_LINE_2[] PROGMEM = "MiniOS SD test line 2";
const char EXPECTED_FIRST_CONTENT[] PROGMEM = "MiniOS SD test line 1\n";
const char EXPECTED_FINAL_CONTENT[] PROGMEM =
    "MiniOS SD test line 1\n"
    "MiniOS SD test line 2\n";

void restore_terminal()
{
    // Every File object is closed by the caller before this point.
    filesystem_end_session();
    terminal_redraw();
}

void print_sd_init_failure()
{
    console_println(F("SD init: FAILED"));
    console_println(
        F("Check card insertion, FAT32 format, SD_CS D4, and shared SPI wiring."));
}

bool write_test_line(File &file, PGM_P line)
{
    for (PGM_P cursor = line;; ++cursor) {
        const uint8_t character = pgm_read_byte(cursor);
        if (character == 0U) {
            break;
        }
        if (file.write(character) != 1U) {
            return false;
        }
    }
    if (file.write(static_cast<uint8_t>('\n')) != 1U) {
        return false;
    }
    file.flush();
    return file.getWriteError() == 0;
}

bool content_matches(File &file, PGM_P expected)
{
    for (PGM_P cursor = expected;; ++cursor) {
        const uint8_t wanted = pgm_read_byte(cursor);
        if (wanted == 0U) {
            // Exact verification rejects trailing bytes as well as a prefix
            // mismatch. Test data uses LF deliberately, independent of the
            // Print::println() CRLF convention.
            return file.read() == -1;
        }
        if (file.read() != static_cast<int>(wanted)) {
            return false;
        }
    }
}

void finish_test(bool passed, const __FlashStringHelper *failed_stage)
{
    restore_terminal();
    console_println(passed ? F("SD test result: PASS") : F("SD test result: FAIL"));
    if (!passed) {
        console_print(F("Failed stage: "));
        console_println(failed_stage);
    }
    // Draw the final result immediately even though the shell batches command
    // output; the prompt is added normally when the handler returns.
    terminal_redraw();
}

bool fail_test(
    const __FlashStringHelper *message, const __FlashStringHelper *failed_stage)
{
    console_println(message);
    finish_test(false, failed_stage);
    return false;
}

}  // namespace

bool sd_initialize()
{
    // Diagnostics deliberately start a fresh session so their initialization
    // report remains deterministic. General filesystem commands keep their
    // verified mount ready between commands.
    filesystem_end_session();
    const bool initialized = filesystem_ensure_ready();
    if (filesystem_last_init_tried_fallback()) {
        console_print(F("SD init: retrying at "));
        console_print_u32(SD_FALLBACK_SPI_HZ);
        console_println(F(" Hz"));
    }
    if (initialized) {
        console_println(F("SD init: OK"));
        if (filesystem_last_init_used_fallback()) {
            console_print(F("SD speed fallback: "));
            console_print_u32(SD_FALLBACK_SPI_HZ);
            console_println(F(" Hz"));
        }
        return true;
    }

    print_sd_init_failure();
    return false;
}

bool sd_print_info()
{
    if (!sd_initialize()) {
        restore_terminal();
        return false;
    }

    filesystem_prepare_access();
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        root.close();
        console_println(F("Root directory open failed"));
        restore_terminal();
        return false;
    }

    console_println(F("Root directory:"));
    bool found_entry = false;
    while (true) {
        filesystem_prepare_access();
        File entry = root.openNextFile();
        if (!entry) {
            entry.close();
            break;
        }

        char entry_name[13];
        strncpy(entry_name, entry.name(), sizeof(entry_name) - 1U);
        entry_name[sizeof(entry_name) - 1U] = '\0';
        const bool is_directory = entry.isDirectory();
        const uint32_t file_size = is_directory ? 0UL : entry.size();
        entry.close();

        found_entry = true;
        console_print(is_directory ? F("[DIR ] ") : F("[FILE] "));
        console_print(entry_name);
        if (!is_directory) {
            console_print(F(" "));
            console_print_u32(file_size);
            console_print(F(" bytes"));
        }
        console_println();
    }
    root.close();

    if (!found_entry) {
        console_println(F("(empty)"));
    }
    console_println(F("SD info complete"));
    restore_terminal();
    return true;
}

bool sd_run_read_write_test()
{
    if (!sd_initialize()) {
        finish_test(false, F("initialization"));
        return false;
    }

    filesystem_prepare_access();
    if (SD.exists(TEST_FILE_PATH)) {
        filesystem_prepare_access();
        if (!SD.remove(TEST_FILE_PATH)) {
            return fail_test(F("Stale test file removal failed"), F("stale file removal"));
        }
        console_println(F("Removed stale /SDTEST.TXT"));
    }

    filesystem_prepare_access();
    File created = SD.open(TEST_FILE_PATH, FILE_WRITE);
    if (!created) {
        created.close();
        return fail_test(F("Test file creation failed"), F("create/write"));
    }
    const bool first_write_ok = write_test_line(created, TEST_LINE_1);
    created.close();
    if (!first_write_ok) {
        return fail_test(F("Test file write failed"), F("create/write"));
    }
    console_println(F("Create/write: OK"));

    filesystem_prepare_access();
    File first_read = SD.open(TEST_FILE_PATH, FILE_READ);
    if (!first_read) {
        first_read.close();
        return fail_test(F("Test file reopen failed"), F("read-back verification"));
    }
    const bool first_content_ok = content_matches(first_read, EXPECTED_FIRST_CONTENT);
    first_read.close();
    if (!first_content_ok) {
        return fail_test(F("Read-back mismatch"), F("read-back verification"));
    }
    console_println(F("Read-back verification: OK"));

    filesystem_prepare_access();
    File appended = SD.open(TEST_FILE_PATH, FILE_WRITE);
    if (!appended) {
        appended.close();
        return fail_test(F("Append open failed"), F("append"));
    }
    const bool append_ok = write_test_line(appended, TEST_LINE_2);
    appended.close();
    if (!append_ok) {
        return fail_test(F("Append failed"), F("append"));
    }
    console_println(F("Append: OK"));

    filesystem_prepare_access();
    File final_read = SD.open(TEST_FILE_PATH, FILE_READ);
    if (!final_read) {
        final_read.close();
        return fail_test(F("Test file reopen failed"), F("append verification"));
    }
    const uint32_t final_size = final_read.size();
    const bool final_content_ok = content_matches(final_read, EXPECTED_FINAL_CONTENT);
    final_read.close();
    if (!final_content_ok) {
        return fail_test(F("Final content mismatch"), F("append verification"));
    }
    console_println(F("Append verification: OK"));
    console_print(F("Final file size: "));
    console_print_u32(final_size);
    console_println(F(" bytes"));

    filesystem_prepare_access();
    if (!SD.remove(TEST_FILE_PATH)) {
        return fail_test(F("Test file delete failed"), F("delete"));
    }
    filesystem_prepare_access();
    if (SD.exists(TEST_FILE_PATH)) {
        return fail_test(F("Delete verification failed"), F("delete verification"));
    }
    console_println(F("Delete verification: OK"));

    finish_test(true, nullptr);
    return true;
}
