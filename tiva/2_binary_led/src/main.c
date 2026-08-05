#include <stdint.h>

#include "gpio.h"
#include "systick.h"

static void FourbitBinaryLED_counter(void)
{
    uint32_t counter = 0U;

    /* Illuminate all LEDs briefly to verify the four output connections. */
    gpio_binary_led_write(0x0FU);
    systick_wait_ms(1000U);
    gpio_binary_led_write(0U);

    for (;;) {
        if (gpio_sw1_pressed_event()) {
            counter = (counter + 1U) & 0x0FU;
            gpio_binary_led_write(counter);
        }
    }
}


static void LED_train(void)
{
    uint32_t array[] = {1, 2, 4, 8};

    /* Illuminate all LEDs briefly to verify the four output connections. */
    gpio_binary_led_write(0x0FU);
    systick_wait_ms(1000U);
    gpio_binary_led_write(0U);

    for (;;) {
        for(uint32_t i = 0; i<(sizeof(array) / sizeof(array[0])); i++)  {
            gpio_binary_led_write(array[i]);
            systick_wait_ms(300U);
        }
    }
}




int main(void)
{
    systick_init();
    gpio_init();
    // LED_train();
    FourbitBinaryLED_counter();

    for (;;) {
    }
}
