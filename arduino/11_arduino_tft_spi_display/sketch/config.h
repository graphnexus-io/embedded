#ifndef MINIOS_CONFIG_H
#define MINIOS_CONFIG_H

#include <Arduino.h>

#define FIRMWARE_NAME "MiniOS Arduino Prototype"
#define FIRMWARE_VERSION "0.6.0"

constexpr uint32_t SERIAL_BAUD_RATE = 115200UL;

constexpr uint8_t LCD_CS_PIN = 10U;
constexpr uint8_t LCD_RS_PIN = 9U;
constexpr uint8_t LCD_RST_PIN = 8U;
constexpr uint8_t LCD_LED_PIN = 5U;
constexpr uint8_t SD_CS_PIN = 4U;
constexpr uint8_t HW_SS_PIN = 53U;

// SD.begin() defaults to 4 MHz after low-speed card negotiation in Arduino
// SD 1.3.0. A failed default attempt is retried at this lower data speed.
constexpr uint32_t SD_FALLBACK_SPI_HZ = 2000000UL;

// The installed SD 1.3.0 library supports FAT 8.3 path components. Paths may
// contain several components, but normalized text is bounded explicitly.
constexpr size_t FS_MAX_PATH = 64U;
constexpr size_t FS_IO_BUFFER_SIZE = 64U;
constexpr uint32_t CAT_MAX_BYTES = 4096UL;

// The modal editor keeps one small complete text file in SRAM. The final byte
// is reserved for a terminator, so files may contain at most 1024 characters.
constexpr uint16_t TEXT_EDITOR_MAX_BYTES = 1024U;

constexpr size_t COMMAND_BUFFER_SIZE = 96U;
constexpr uint8_t COMMAND_MAX_ARGUMENTS = 12U;
constexpr uint8_t COMMANDS_PER_LINE = 6U;
constexpr uint8_t HISTORY_ENTRY_COUNT = 8U;
constexpr uint8_t HISTORY_ENTRY_SIZE = 64U;
constexpr uint8_t TERMINAL_VISIBLE_LINES = 30U;
constexpr uint8_t TERMINAL_LINE_LENGTH = 76U;
constexpr uint8_t TEXT_EDITOR_CONTENT_ROWS = TERMINAL_VISIBLE_LINES - 2U;

#define SHELL_PROMPT "i: "

#endif
