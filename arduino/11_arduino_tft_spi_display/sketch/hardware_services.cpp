#include "hardware_services.h"

#include <avr/wdt.h>
#include <string.h>

#include "config.h"

// Run before normal C++ initialization so a watchdog-triggered reset cannot
// loop before setup() gets a chance to execute. This follows the avr-libc
// watchdog startup pattern and is isolated here for the future platform port.
void disable_watchdog_early() __attribute__((naked, section(".init3"), used));
void disable_watchdog_early()
{
    MCUSR = 0U;
    wdt_disable();
}

namespace {

uint8_t output_pins[(NUM_DIGITAL_PINS + 7U) / 8U];
uint8_t backlight_level = 255U;

bool pin_is_output(uint8_t pin)
{
    return (output_pins[pin / 8U] & static_cast<uint8_t>(1U << (pin % 8U))) != 0U;
}

void set_output_state(uint8_t pin, bool output)
{
    const uint8_t mask = static_cast<uint8_t>(1U << (pin % 8U));
    if (output) {
        output_pins[pin / 8U] |= mask;
    } else {
        output_pins[pin / 8U] &= static_cast<uint8_t>(~mask);
    }
}

}  // namespace

void hardware_services_init()
{
    // Repeat defensively after Arduino core startup.
    MCUSR &= static_cast<uint8_t>(~_BV(WDRF));
    wdt_disable();
    memset(output_pins, 0, sizeof(output_pins));

    // Keep both shared-SPI peripherals deselected until their libraries take
    // ownership. D53 must stay an output or AVR hardware SPI can leave master
    // mode even though it is not used as a peripheral chip select here.
    pinMode(LCD_CS_PIN, OUTPUT);
    pinMode(SD_CS_PIN, OUTPUT);
    pinMode(HW_SS_PIN, OUTPUT);
    digitalWrite(LCD_CS_PIN, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    digitalWrite(HW_SS_PIN, HIGH);
}

uint16_t hardware_free_sram()
{
    // Instantaneous estimate: bytes between the current heap end and a local
    // stack address. Measuring performs no allocation.
    extern char __heap_start;
    extern char *__brkval;
    char stack_marker;
    const char *heap_end = (__brkval == nullptr) ? &__heap_start : __brkval;
    return static_cast<uint16_t>(
        reinterpret_cast<uintptr_t>(&stack_marker) - reinterpret_cast<uintptr_t>(heap_end));
}

uint8_t hardware_backlight_level()
{
    return backlight_level;
}

void hardware_set_backlight(uint8_t level)
{
    // D5 is reserved from generic GPIO access. analogWrite() uses its hardware
    // PWM channel; levels 0 and 255 become steady LOW and HIGH respectively.
    analogWrite(LCD_LED_PIN, level);
    backlight_level = level;
}

HardwarePinReservation hardware_pin_reservation(uint16_t pin)
{
    if (pin == 0U || pin == 1U) {
        return PIN_RESERVED_UART;
    }
    if (pin == SD_CS_PIN) {
        return PIN_RESERVED_SD;
    }
    if (pin == LCD_LED_PIN || pin == LCD_RST_PIN || pin == LCD_RS_PIN ||
        pin == LCD_CS_PIN) {
        return PIN_RESERVED_DISPLAY;
    }
    if (pin == 50U || pin == 51U || pin == 52U || pin == HW_SS_PIN) {
        return PIN_RESERVED_SPI;
    }
    return PIN_AVAILABLE;
}

HardwareGpioStatus hardware_gpio_mode(uint16_t pin, HardwareGpioMode mode)
{
    if (pin >= NUM_DIGITAL_PINS) {
        return GPIO_INVALID_PIN;
    }
    if (hardware_pin_reservation(pin) != PIN_AVAILABLE) {
        return GPIO_RESERVED_PIN;
    }

    uint8_t arduino_mode = INPUT;
    if (mode == GPIO_MODE_INPUT_PULLUP) {
        arduino_mode = INPUT_PULLUP;
    } else if (mode == GPIO_MODE_OUTPUT) {
        arduino_mode = OUTPUT;
    }
    pinMode(static_cast<uint8_t>(pin), arduino_mode);
    set_output_state(static_cast<uint8_t>(pin), mode == GPIO_MODE_OUTPUT);
    return GPIO_OK;
}

HardwareGpioStatus hardware_gpio_read(uint16_t pin, uint8_t *value)
{
    if (pin >= NUM_DIGITAL_PINS || value == nullptr) {
        return GPIO_INVALID_PIN;
    }
    if (hardware_pin_reservation(pin) != PIN_AVAILABLE) {
        return GPIO_RESERVED_PIN;
    }
    *value = static_cast<uint8_t>(digitalRead(static_cast<uint8_t>(pin)) == HIGH);
    return GPIO_OK;
}

HardwareGpioStatus hardware_gpio_write(uint16_t pin, uint8_t value)
{
    if (pin >= NUM_DIGITAL_PINS) {
        return GPIO_INVALID_PIN;
    }
    if (hardware_pin_reservation(pin) != PIN_AVAILABLE) {
        return GPIO_RESERVED_PIN;
    }
    if (!pin_is_output(static_cast<uint8_t>(pin))) {
        return GPIO_NOT_OUTPUT;
    }
    digitalWrite(static_cast<uint8_t>(pin), value == 0U ? LOW : HIGH);
    return GPIO_OK;
}

void hardware_reboot()
{
    Serial.flush();
    delay(20U);
    wdt_enable(WDTO_15MS);
    while (true) {
        // Wait for the watchdog reset; do not jump to the reset vector.
    }
}
