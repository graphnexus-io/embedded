#include "gpio.h"
#include "systick.h"
#include "tm4c123_registers.h"

#define BUTTON_DEBOUNCE_MS 20U

static bool gpio_sw1_raw_is_pressed(void)
{
    return (GPIO_PORTF_DATA_R & PF4_BIT) == 0U;
}

void gpio_init(void)
{
    const uint32_t port_clocks =
        GPIO_PORTB_CLOCK_BIT | GPIO_PORTF_CLOCK_BIT;

    SYSCTL_RCGCGPIO_R |= port_clocks;
    while ((SYSCTL_PRGPIO_R & port_clocks) != port_clocks) {
    }

    /* PB0-PB3: four-bit LED output. */
    GPIO_PORTB_AMSEL_R &= ~BINARY_LED_MASK;
    GPIO_PORTB_AFSEL_R &= ~BINARY_LED_MASK;
    GPIO_PORTB_PCTL_R  &= ~0x0000FFFFU;
    GPIO_PORTB_DATA_R  &= ~BINARY_LED_MASK;
    GPIO_PORTB_DIR_R   |= BINARY_LED_MASK;
    GPIO_PORTB_DEN_R   |= BINARY_LED_MASK;

    /* PF4/SW1: active-low digital input with an internal pull-up. */
    GPIO_PORTF_AMSEL_R &= ~PF4_BIT;
    GPIO_PORTF_AFSEL_R &= ~PF4_BIT;
    GPIO_PORTF_PCTL_R  &= ~(0xFU << 16);
    GPIO_PORTF_DIR_R   &= ~PF4_BIT;
    GPIO_PORTF_DEN_R   |= PF4_BIT;
    GPIO_PORTF_PUR_R   |= PF4_BIT;
}

void gpio_binary_led_write(uint32_t value)
{
    GPIO_PORTB_DATA_R =
        (GPIO_PORTB_DATA_R & ~BINARY_LED_MASK) |
        (value & BINARY_LED_MASK);
}

bool gpio_sw1_pressed_event(void)
{
    static bool stable_pressed = false;
    bool sampled_pressed = gpio_sw1_raw_is_pressed();

    if (sampled_pressed == stable_pressed) {
        return false;
    }

    systick_wait_ms(BUTTON_DEBOUNCE_MS);
    sampled_pressed = gpio_sw1_raw_is_pressed();

    if (sampled_pressed == stable_pressed) {
        return false;
    }

    stable_pressed = sampled_pressed;
    return stable_pressed;
}
