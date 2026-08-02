#include "commands.h"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "command_shell.h"
#include "commands_filesystem.h"
#include "config.h"
#include "console.h"
#include "hardware_services.h"
#include "sd_diagnostics.h"
#include "tft_terminal.h"

namespace {

using CommandHandler = CommandStatus (*)(int argc, char *argv[]);

struct CommandEntry {
    PGM_P name;
    CommandHandler handler;
    PGM_P description;
    PGM_P usage;
    PGM_P details;
};

CommandStatus command_help(int argc, char *argv[]);
CommandStatus command_clear(int argc, char *argv[]);
CommandStatus command_version(int argc, char *argv[]);
CommandStatus command_uptime(int argc, char *argv[]);
CommandStatus command_info(int argc, char *argv[]);
CommandStatus command_history(int argc, char *argv[]);
CommandStatus command_mem(int argc, char *argv[]);
CommandStatus command_backlight(int argc, char *argv[]);
CommandStatus command_sdinfo(int argc, char *argv[]);
CommandStatus command_sdtest(int argc, char *argv[]);
CommandStatus command_reboot(int argc, char *argv[]);
CommandStatus command_gpio(int argc, char *argv[]);
CommandStatus command_display(int argc, char *argv[]);
CommandStatus command_calc(int argc, char *argv[]);

#define COMMAND_TEXT(symbol, value) const char symbol[] PROGMEM = value

COMMAND_TEXT(name_help, "help");
COMMAND_TEXT(name_echo, "echo");
COMMAND_TEXT(name_pwd, "pwd");
COMMAND_TEXT(name_cd, "cd");
COMMAND_TEXT(name_ls, "ls");
COMMAND_TEXT(name_cat, "cat");
COMMAND_TEXT(name_mkdir, "mkdir");
COMMAND_TEXT(name_rmdir, "rmdir");
COMMAND_TEXT(name_rm, "rm");
COMMAND_TEXT(name_mv, "mv");
COMMAND_TEXT(name_nano, "nano");
COMMAND_TEXT(name_clear, "clear");
COMMAND_TEXT(name_clear_alias, "cl");
COMMAND_TEXT(name_version, "version");
COMMAND_TEXT(name_uptime, "uptime");
COMMAND_TEXT(name_info, "info");
COMMAND_TEXT(name_history, "history");
COMMAND_TEXT(name_mem, "mem");
COMMAND_TEXT(name_backlight, "backlight");
COMMAND_TEXT(name_sdinfo, "sdinfo");
COMMAND_TEXT(name_sdtest, "sdtest");
COMMAND_TEXT(name_reboot, "reboot");
COMMAND_TEXT(name_gpio, "gpio");
COMMAND_TEXT(name_display, "display");
COMMAND_TEXT(name_calc, "calc");

COMMAND_TEXT(desc_help, "show command help");
COMMAND_TEXT(desc_echo, "print text or write text to a file");
COMMAND_TEXT(desc_pwd, "print the working directory");
COMMAND_TEXT(desc_cd, "change the working directory");
COMMAND_TEXT(desc_ls, "list directory entries");
COMMAND_TEXT(desc_cat, "print a text file");
COMMAND_TEXT(desc_mkdir, "create a directory");
COMMAND_TEXT(desc_rmdir, "remove an empty directory");
COMMAND_TEXT(desc_rm, "remove a file");
COMMAND_TEXT(desc_mv, "move a regular file");
COMMAND_TEXT(desc_nano, "edit a small text file");
COMMAND_TEXT(desc_clear, "clear Serial and TFT terminals");
COMMAND_TEXT(desc_version, "show firmware version");
COMMAND_TEXT(desc_uptime, "show elapsed time");
COMMAND_TEXT(desc_info, "show platform information");
COMMAND_TEXT(desc_history, "show command history");
COMMAND_TEXT(desc_mem, "show an SRAM estimate");
COMMAND_TEXT(desc_backlight, "show or set TFT backlight intensity");
COMMAND_TEXT(desc_sdinfo, "initialize SD and list the root directory");
COMMAND_TEXT(desc_sdtest, "test /SDTEST.TXT create/read/append/delete");
COMMAND_TEXT(desc_reboot, "reset using the AVR watchdog");
COMMAND_TEXT(desc_gpio, "inspect and control safe GPIO pins");
COMMAND_TEXT(desc_display, "run TFT diagnostics");
COMMAND_TEXT(desc_calc, "evaluate one integer operation");

COMMAND_TEXT(usage_help, "help [command]");
COMMAND_TEXT(usage_echo, "echo [text...] [> file | >> file]");
COMMAND_TEXT(usage_pwd, "pwd");
COMMAND_TEXT(usage_cd, "cd [directory]");
COMMAND_TEXT(usage_ls, "ls [directory]");
COMMAND_TEXT(usage_cat, "cat <file>");
COMMAND_TEXT(usage_mkdir, "mkdir <directory>");
COMMAND_TEXT(usage_rmdir, "rmdir <directory>");
COMMAND_TEXT(usage_rm, "rm <file>");
COMMAND_TEXT(usage_mv, "mv <source> <destination>");
COMMAND_TEXT(usage_nano, "nano <file>");
COMMAND_TEXT(usage_clear, "clear");
COMMAND_TEXT(usage_version, "version");
COMMAND_TEXT(usage_uptime, "uptime");
COMMAND_TEXT(usage_info, "info");
COMMAND_TEXT(usage_history, "history");
COMMAND_TEXT(usage_mem, "mem");
COMMAND_TEXT(usage_backlight, "backlight [0-255]");
COMMAND_TEXT(usage_sdinfo, "sdinfo");
COMMAND_TEXT(usage_sdtest, "sdtest");
COMMAND_TEXT(usage_reboot, "reboot");
COMMAND_TEXT(usage_gpio, "gpio mode <pin> input|input_pullup|output\n"
                         "       gpio read <pin>\n"
                         "       gpio write <pin> 0|1|low|high");
COMMAND_TEXT(usage_display, "display clear|info|benchmark\n"
                            "       display fill <colour|rgb565>");
COMMAND_TEXT(usage_calc, "calc <value> <operator> <value>");

COMMAND_TEXT(detail_help, "example: help calc");
COMMAND_TEXT(detail_echo, "standalone > overwrites; >> appends; count includes newline\n"
                          "example: echo \"hello\" > NOTES.TXT");
COMMAND_TEXT(detail_pwd, "prints the absolute FAT path held in fixed memory");
COMMAND_TEXT(detail_cd, "no argument selects /; supports absolute, relative, . and ..\n"
                        "example: cd /DATA");
COMMAND_TEXT(detail_ls, "defaults to the working directory and lists one level\n"
                        "example: ls /DATA");
COMMAND_TEXT(detail_cat, "prints at most 4096 bytes; non-text bytes become '.'\n"
                         "example: cat NOTES.TXT");
COMMAND_TEXT(detail_mkdir, "creates one directory; its parent must already exist\n"
                           "example: mkdir DATA");
COMMAND_TEXT(detail_rmdir, "refuses / and non-empty directories\n"
                           "example: rmdir DATA");
COMMAND_TEXT(detail_rm, "removes one regular file; no recursion or wildcards\n"
                        "example: rm NOTES.TXT");
COMMAND_TEXT(detail_mv, "copy/verify/remove; refuses overwrite and directories\n"
                        "example: mv OLD.TXT DATA/NEW.TXT");
COMMAND_TEXT(detail_nano, "1024-byte text editor; ^O saves; Esc exits\n"
                          "example: nano CONFIG.JSN");
COMMAND_TEXT(detail_clear, "sends ANSI clear-screen and resets the TFT terminal");
COMMAND_TEXT(detail_version, "no arguments");
COMMAND_TEXT(detail_uptime, "elapsed seconds from millis()");
COMMAND_TEXT(detail_info, "board, MCU, display, serial, touch and storage status");
COMMAND_TEXT(detail_history, "entries are shown oldest first; Up/Down recalls them");
COMMAND_TEXT(detail_mem, "free SRAM is an instantaneous stack-to-heap estimate");
COMMAND_TEXT(detail_backlight, "uses PWM on reserved TFT backlight pin D5");
COMMAND_TEXT(detail_sdinfo, "non-destructive; lists root entries only");
COMMAND_TEXT(detail_sdtest, "modifies and deletes only /SDTEST.TXT");
COMMAND_TEXT(detail_reboot, "serial output is flushed before watchdog reset");
COMMAND_TEXT(detail_gpio, "reserved UART, TFT, SD and SPI pins are always denied");
COMMAND_TEXT(detail_display, "colours: black white red green blue yellow\n"
                             "example: display fill 0xF800");
COMMAND_TEXT(detail_calc, "operators: + - * / % & | ^ << >>\n"
                          "values: signed decimal or 0x hexadecimal");

const CommandEntry command_table[] PROGMEM = {
    {name_help, command_help, desc_help, usage_help, detail_help},
    {name_echo, command_fs_echo, desc_echo, usage_echo, detail_echo},
    {name_pwd, command_fs_pwd, desc_pwd, usage_pwd, detail_pwd},
    {name_cd, command_fs_cd, desc_cd, usage_cd, detail_cd},
    {name_ls, command_fs_ls, desc_ls, usage_ls, detail_ls},
    {name_cat, command_fs_cat, desc_cat, usage_cat, detail_cat},
    {name_mkdir, command_fs_mkdir, desc_mkdir, usage_mkdir, detail_mkdir},
    {name_rmdir, command_fs_rmdir, desc_rmdir, usage_rmdir, detail_rmdir},
    {name_rm, command_fs_rm, desc_rm, usage_rm, detail_rm},
    {name_mv, command_fs_mv, desc_mv, usage_mv, detail_mv},
    {name_nano, command_fs_nano, desc_nano, usage_nano, detail_nano},
    {name_clear, command_clear, desc_clear, usage_clear, detail_clear},
    {name_clear_alias, command_clear, desc_clear, usage_clear, detail_clear},
    {name_version, command_version, desc_version, usage_version, detail_version},
    {name_uptime, command_uptime, desc_uptime, usage_uptime, detail_uptime},
    {name_info, command_info, desc_info, usage_info, detail_info},
    {name_history, command_history, desc_history, usage_history, detail_history},
    {name_mem, command_mem, desc_mem, usage_mem, detail_mem},
    {name_backlight, command_backlight, desc_backlight, usage_backlight, detail_backlight},
    {name_sdinfo, command_sdinfo, desc_sdinfo, usage_sdinfo, detail_sdinfo},
    {name_sdtest, command_sdtest, desc_sdtest, usage_sdtest, detail_sdtest},
    {name_reboot, command_reboot, desc_reboot, usage_reboot, detail_reboot},
    {name_gpio, command_gpio, desc_gpio, usage_gpio, detail_gpio},
    {name_display, command_display, desc_display, usage_display, detail_display},
    {name_calc, command_calc, desc_calc, usage_calc, detail_calc},
};

constexpr uint8_t COMMAND_COUNT = sizeof(command_table) / sizeof(command_table[0]);

PGM_P table_string(uint8_t index, PGM_P CommandEntry::*field)
{
    return reinterpret_cast<PGM_P>(pgm_read_ptr(&(command_table[index].*field)));
}

void print_progmem(PGM_P text)
{
    console_print(reinterpret_cast<const __FlashStringHelper *>(text));
}

void println_progmem(PGM_P text)
{
    console_println(reinterpret_cast<const __FlashStringHelper *>(text));
}

int8_t find_command(const char *name)
{
    for (uint8_t index = 0U; index < COMMAND_COUNT; ++index) {
        PGM_P entry_name = table_string(index, &CommandEntry::name);
        if (strcmp_P(name, entry_name) == 0) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

void print_usage(PGM_P usage)
{
    console_print(F("Usage: "));
    println_progmem(usage);
}

bool parse_i32(const char *text, int32_t *value)
{
    if (text == nullptr || *text == '\0' || value == nullptr) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const long parsed = strtol(text, &end, 0);
    if (errno == ERANGE || end == text || *end != '\0') {
        return false;
    }
    *value = static_cast<int32_t>(parsed);
    return true;
}

bool parse_u32(const char *text, uint8_t base, uint32_t *value)
{
    if (text == nullptr || *text == '\0' || *text == '-' || *text == '+' || value == nullptr) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long parsed = strtoul(text, &end, base);
    if (errno == ERANGE || end == text || *end != '\0') {
        return false;
    }
    *value = static_cast<uint32_t>(parsed);
    return true;
}

CommandStatus reject_extra_arguments(int argc, PGM_P usage)
{
    if (argc == 1) {
        return COMMAND_OK;
    }
    print_usage(usage);
    return COMMAND_BAD_ARGUMENTS;
}

CommandStatus command_help(int argc, char *argv[])
{
    if (argc > 2) {
        print_usage(usage_help);
        return COMMAND_BAD_ARGUMENTS;
    }
    if (argc == 1) {
        console_println(F("Commands:"));
        for (uint8_t index = 0U; index < COMMAND_COUNT; ++index) {
            console_print(F("  "));
            print_progmem(table_string(index, &CommandEntry::name));
            console_print(F(" - "));
            println_progmem(table_string(index, &CommandEntry::description));
        }
        return COMMAND_OK;
    }

    const int8_t index = find_command(argv[1]);
    if (index < 0) {
        console_print(F("No such command: "));
        console_println(argv[1]);
        return COMMAND_NOT_FOUND;
    }
    const uint8_t command_index = static_cast<uint8_t>(index);
    print_progmem(table_string(command_index, &CommandEntry::name));
    console_print(F(" - "));
    println_progmem(table_string(command_index, &CommandEntry::description));
    console_print(F("usage: "));
    println_progmem(table_string(command_index, &CommandEntry::usage));
    println_progmem(table_string(command_index, &CommandEntry::details));
    return COMMAND_OK;
}

CommandStatus command_clear(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_clear);
    if (status != COMMAND_OK) {
        return status;
    }
    Serial.print(F("\x1b[2J\x1b[H"));
    terminal_clear();
    return COMMAND_OK;
}

CommandStatus command_version(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_version);
    if (status != COMMAND_OK) {
        return status;
    }
    console_print(F(FIRMWARE_NAME " "));
    console_println(FIRMWARE_VERSION);
    return COMMAND_OK;
}

CommandStatus command_uptime(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_uptime);
    if (status != COMMAND_OK) {
        return status;
    }
    console_print(F("Uptime: "));
    console_print_u32(static_cast<uint32_t>(millis()) / 1000UL);
    console_println(F(" s"));
    return COMMAND_OK;
}

CommandStatus command_info(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_info);
    if (status != COMMAND_OK) {
        return status;
    }
    console_println(F("Platform: Arduino Mega 2560"));
    console_println(F("MCU: ATmega2560"));
    console_print(F("CPU clock: "));
    console_print_u32(static_cast<uint32_t>(F_CPU));
    console_println(F(" Hz"));
    console_println(F("Display: ST7796S 480x320"));
    console_print(F("Serial: "));
    console_print_u32(SERIAL_BAUD_RATE);
    console_println(F(" baud"));
    console_println(F("Touch: not initialized"));
    console_println(F("Storage: FAT filesystem commands and SD diagnostics"));
    return COMMAND_OK;
}

CommandStatus command_history(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_history);
    if (status != COMMAND_OK) {
        return status;
    }
    const uint8_t count = shell_history_count();
    for (uint8_t index = 0U; index < count; ++index) {
        console_print_u32(static_cast<uint32_t>(index + 1U));
        console_print(F("  "));
        console_println(shell_history_get(index));
    }
    return COMMAND_OK;
}

CommandStatus command_mem(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_mem);
    if (status != COMMAND_OK) {
        return status;
    }
    console_println(F("SRAM total: 8192 bytes"));
    console_print(F("SRAM free: "));
    console_print_u32(hardware_free_sram());
    console_println(F(" bytes"));
    return COMMAND_OK;
}

CommandStatus command_backlight(int argc, char *argv[])
{
    if (argc > 2) {
        print_usage(usage_backlight);
        return COMMAND_BAD_ARGUMENTS;
    }
    if (argc == 2) {
        uint32_t level = 0UL;
        if (!parse_u32(argv[1], 10U, &level) || level > 255UL) {
            console_println(F("Error: backlight level must be 0..255"));
            print_usage(usage_backlight);
            return COMMAND_BAD_ARGUMENTS;
        }
        hardware_set_backlight(static_cast<uint8_t>(level));
    }

    console_print(F("Backlight: "));
    console_print_u32(hardware_backlight_level());
    console_println(F("/255"));
    return COMMAND_OK;
}

CommandStatus command_sdinfo(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_sdinfo);
    if (status != COMMAND_OK) {
        return status;
    }
    return sd_print_info() ? COMMAND_OK : COMMAND_FAILED;
}

CommandStatus command_sdtest(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_sdtest);
    if (status != COMMAND_OK) {
        return status;
    }
    return sd_run_read_write_test() ? COMMAND_OK : COMMAND_FAILED;
}

CommandStatus command_reboot(int argc, char *[])
{
    const CommandStatus status = reject_extra_arguments(argc, usage_reboot);
    if (status != COMMAND_OK) {
        return status;
    }
    console_println(F("Rebooting..."));
    terminal_redraw();
    hardware_reboot();
}

void print_reserved_pin(uint16_t pin)
{
    console_print(F("Error: pin "));
    console_print_u32(pin);
    const HardwarePinReservation reservation = hardware_pin_reservation(pin);
    if (reservation == PIN_RESERVED_UART) {
        console_println(F(" is reserved by USB serial"));
    } else if (reservation == PIN_RESERVED_DISPLAY) {
        console_println(F(" is reserved by TFT display"));
    } else if (reservation == PIN_RESERVED_SD) {
        console_println(F(" is reserved by SD card chip select"));
    } else {
        console_println(F(" is reserved by shared SPI bus"));
    }
}

CommandStatus gpio_result(HardwareGpioStatus result, uint16_t pin)
{
    if (result == GPIO_OK) {
        return COMMAND_OK;
    }
    if (result == GPIO_INVALID_PIN) {
        console_println(F("Error: invalid pin"));
        return COMMAND_BAD_ARGUMENTS;
    }
    if (result == GPIO_RESERVED_PIN) {
        print_reserved_pin(pin);
        return COMMAND_DENIED;
    }
    console_println(F("Error: pin is not configured as output"));
    return COMMAND_DENIED;
}

CommandStatus command_gpio(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage(usage_gpio);
        return COMMAND_BAD_ARGUMENTS;
    }
    uint32_t parsed_pin = 0UL;
    if (!parse_u32(argv[2], 10U, &parsed_pin) || parsed_pin >= NUM_DIGITAL_PINS) {
        console_println(F("Error: invalid pin"));
        return COMMAND_BAD_ARGUMENTS;
    }
    const uint16_t pin = static_cast<uint16_t>(parsed_pin);

    if (strcmp(argv[1], "mode") == 0) {
        if (argc != 4) {
            print_usage(usage_gpio);
            return COMMAND_BAD_ARGUMENTS;
        }
        HardwareGpioMode mode;
        if (strcmp(argv[3], "input") == 0) {
            mode = GPIO_MODE_INPUT;
        } else if (strcmp(argv[3], "input_pullup") == 0) {
            mode = GPIO_MODE_INPUT_PULLUP;
        } else if (strcmp(argv[3], "output") == 0) {
            mode = GPIO_MODE_OUTPUT;
        } else {
            console_println(F("Error: unsupported GPIO mode"));
            return COMMAND_BAD_ARGUMENTS;
        }
        const CommandStatus status = gpio_result(hardware_gpio_mode(pin, mode), pin);
        if (status == COMMAND_OK) {
            console_println(F("OK"));
        }
        return status;
    }

    if (strcmp(argv[1], "read") == 0) {
        if (argc != 3) {
            print_usage(usage_gpio);
            return COMMAND_BAD_ARGUMENTS;
        }
        uint8_t value = 0U;
        const CommandStatus status = gpio_result(hardware_gpio_read(pin, &value), pin);
        if (status == COMMAND_OK) {
            console_print_u32(pin);
            console_println(value == 0U ? F(": LOW") : F(": HIGH"));
        }
        return status;
    }

    if (strcmp(argv[1], "write") == 0) {
        if (argc != 4) {
            print_usage(usage_gpio);
            return COMMAND_BAD_ARGUMENTS;
        }
        uint8_t value;
        if (strcmp(argv[3], "0") == 0 || strcmp(argv[3], "low") == 0) {
            value = 0U;
        } else if (strcmp(argv[3], "1") == 0 || strcmp(argv[3], "high") == 0) {
            value = 1U;
        } else {
            console_println(F("Error: GPIO value must be 0, 1, low or high"));
            return COMMAND_BAD_ARGUMENTS;
        }
        const CommandStatus status = gpio_result(hardware_gpio_write(pin, value), pin);
        if (status == COMMAND_OK) {
            console_println(F("OK"));
        }
        return status;
    }

    print_usage(usage_gpio);
    return COMMAND_BAD_ARGUMENTS;
}

bool parse_color(const char *text, uint16_t *color)
{
    if (strcmp(text, "black") == 0) {
        *color = 0x0000U;
    } else if (strcmp(text, "white") == 0) {
        *color = 0xFFFFU;
    } else if (strcmp(text, "red") == 0) {
        *color = 0xF800U;
    } else if (strcmp(text, "green") == 0) {
        *color = 0x07E0U;
    } else if (strcmp(text, "blue") == 0) {
        *color = 0x001FU;
    } else if (strcmp(text, "yellow") == 0) {
        *color = 0xFFE0U;
    } else {
        uint32_t numeric = 0UL;
        if (!parse_u32(text, 0U, &numeric) || numeric > 0xFFFFUL) {
            return false;
        }
        *color = static_cast<uint16_t>(numeric);
    }
    return true;
}

void print_milliseconds(const __FlashStringHelper *label, uint32_t microseconds)
{
    console_print(label);
    console_print_u32(microseconds / 1000UL);
    console_println(F(" ms"));
}

CommandStatus command_display(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(usage_display);
        return COMMAND_BAD_ARGUMENTS;
    }
    if (strcmp(argv[1], "clear") == 0 && argc == 2) {
        terminal_clear();
        return COMMAND_OK;
    }
    if (strcmp(argv[1], "info") == 0 && argc == 2) {
        console_println(F("Controller: ST7796S"));
        console_println(F("Resolution: 480x320"));
        console_println(F("Interface: SPI"));
        console_println(F("Rotation: landscape"));
        return COMMAND_OK;
    }
    if (strcmp(argv[1], "fill") == 0 && argc == 3) {
        uint16_t color = 0U;
        if (!parse_color(argv[2], &color)) {
            console_println(F("Error: invalid RGB565 colour"));
            return COMMAND_BAD_ARGUMENTS;
        }
        terminal_demo_fill(color);
        console_println(F("OK"));
        return COMMAND_OK;
    }
    if (strcmp(argv[1], "benchmark") == 0 && argc == 2) {
        TerminalBenchmark result;
        terminal_run_benchmark(&result);
        print_milliseconds(F("Full-screen fill: "), result.full_screen_us);
        print_milliseconds(F("Terminal redraw: "), result.terminal_redraw_us);
        print_milliseconds(F("Text line: "), result.text_line_us);
        return COMMAND_OK;
    }

    print_usage(usage_display);
    return COMMAND_BAD_ARGUMENTS;
}

void calc_usage()
{
    print_usage(usage_calc);
}

CommandStatus calc_error(const __FlashStringHelper *message)
{
    console_println(message);
    calc_usage();
    return COMMAND_FAILED;
}

CommandStatus command_calc(int argc, char *argv[])
{
    if (argc != 4) {
        calc_usage();
        return COMMAND_BAD_ARGUMENTS;
    }

    int32_t left = 0;
    int32_t right = 0;
    if (!parse_i32(argv[1], &left) || !parse_i32(argv[3], &right)) {
        return calc_error(F("Error: invalid integer"));
    }

    const char *op = argv[2];
    int32_t signed_result = 0;
    if (strcmp(op, "+") == 0) {
        if ((right > 0 && left > INT32_MAX - right) ||
            (right < 0 && left < INT32_MIN - right)) {
            return calc_error(F("Error: arithmetic overflow"));
        }
        signed_result = left + right;
    } else if (strcmp(op, "-") == 0) {
        if ((right > 0 && left < INT32_MIN + right) ||
            (right < 0 && left > INT32_MAX + right)) {
            return calc_error(F("Error: arithmetic overflow"));
        }
        signed_result = left - right;
    } else if (strcmp(op, "*") == 0) {
        const int64_t wide = static_cast<int64_t>(left) * static_cast<int64_t>(right);
        if (wide > INT32_MAX || wide < INT32_MIN) {
            return calc_error(F("Error: arithmetic overflow"));
        }
        signed_result = static_cast<int32_t>(wide);
    } else if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        if (right == 0) {
            return calc_error(strcmp(op, "/") == 0 ?
                                  F("Error: division by zero") : F("Error: modulo by zero"));
        }
        if (left == INT32_MIN && right == -1) {
            return calc_error(F("Error: arithmetic overflow"));
        }
        signed_result = strcmp(op, "/") == 0 ? left / right : left % right;
    } else if (strcmp(op, "&") == 0 || strcmp(op, "|") == 0 ||
               strcmp(op, "^") == 0) {
        const uint32_t a = static_cast<uint32_t>(left);
        const uint32_t b = static_cast<uint32_t>(right);
        uint32_t result = a & b;
        if (strcmp(op, "|") == 0) {
            result = a | b;
        } else if (strcmp(op, "^") == 0) {
            result = a ^ b;
        }
        console_println_u32(result);
        return COMMAND_OK;
    } else if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) {
        if (right < 0 || right > 31) {
            return calc_error(F("Error: shift count must be 0..31"));
        }
        const uint32_t value = static_cast<uint32_t>(left);
        const uint8_t shift = static_cast<uint8_t>(right);
        const uint32_t result = strcmp(op, "<<") == 0 ? value << shift : value >> shift;
        console_println_u32(result);
        return COMMAND_OK;
    } else {
        return calc_error(F("Error: unsupported operator"));
    }

    console_println_i32(signed_result);
    return COMMAND_OK;
}

}  // namespace

CommandStatus commands_dispatch(int argc, char *argv[])
{
    if (argc <= 0 || argv == nullptr) {
        return COMMAND_OK;
    }
    const int8_t index = find_command(argv[0]);
    if (index < 0) {
        console_print(F("Unknown command: "));
        console_println(argv[0]);
        console_println(F("Type 'help' for available commands."));
        return COMMAND_NOT_FOUND;
    }

    const CommandHandler handler = reinterpret_cast<CommandHandler>(
        pgm_read_ptr(&command_table[static_cast<uint8_t>(index)].handler));
    return handler(argc, argv);
}
