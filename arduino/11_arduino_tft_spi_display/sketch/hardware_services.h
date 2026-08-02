#ifndef MINIOS_HARDWARE_SERVICES_H
#define MINIOS_HARDWARE_SERVICES_H

#include <Arduino.h>

enum HardwarePinReservation : uint8_t {
    PIN_AVAILABLE = 0,
    PIN_RESERVED_UART,
    PIN_RESERVED_DISPLAY,
    PIN_RESERVED_SD,
    PIN_RESERVED_SPI,
};

enum HardwareGpioMode : uint8_t {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_INPUT_PULLUP,
    GPIO_MODE_OUTPUT,
};

enum HardwareGpioStatus : uint8_t {
    GPIO_OK = 0,
    GPIO_INVALID_PIN,
    GPIO_RESERVED_PIN,
    GPIO_NOT_OUTPUT,
};

void hardware_services_init();
uint16_t hardware_free_sram();
uint8_t hardware_backlight_level();
void hardware_set_backlight(uint8_t level);
HardwarePinReservation hardware_pin_reservation(uint16_t pin);
HardwareGpioStatus hardware_gpio_mode(uint16_t pin, HardwareGpioMode mode);
HardwareGpioStatus hardware_gpio_read(uint16_t pin, uint8_t *value);
HardwareGpioStatus hardware_gpio_write(uint16_t pin, uint8_t value);
void hardware_reboot() __attribute__((noreturn));

#endif
