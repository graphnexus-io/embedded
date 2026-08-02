#include <Arduino.h>

#include "command_shell.h"
#include "config.h"
#include "hardware_services.h"
#include "tft_terminal.h"

void setup()
{
    hardware_services_init();
    Serial.begin(SERIAL_BAUD_RATE);
    terminal_init();

    // Keep identification on USB serial. The TFT intentionally starts as a
    // bare glass terminal with no banner or application chrome.
    Serial.println(F(FIRMWARE_NAME));
    Serial.print(F("Version "));
    Serial.println(F(FIRMWARE_VERSION));
    Serial.println(F("Type 'help' for available commands."));
    Serial.println();

    shell_init();
}

void loop()
{
    // Non-blocking polling keeps the firmware ready for future peripherals.
    shell_poll();
}
