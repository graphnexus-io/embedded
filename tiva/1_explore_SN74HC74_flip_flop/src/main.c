#include "uart0.h"
#include "gpio.h"
#include <stdint.h>

static void delay(volatile uint32_t count) {
    while(count > 0U) {
        __asm volatile ("nop");
        count--;
    }
}


int main(void)
{

    gpio_portb_pb0_init();
    gpio_portb_pb1_init();
    gpio_portf_init();


    uart0_init();
    uart0_puts("\r\nSN74HC74 test firmware ready.\r\n");


    uart0_puts("Data: 0, rising edge\r\n");
    pb0_off();
    pb1_on();
    pb1_off();

    uart0_puts("Delay\r\n");
    delay(5000000U);

    uart0_puts("Data: 1, rising edge\r\n");
    pb0_on();
    pb1_on();
    pb1_off();

    delay(5000000U);


    for (;;) {
        pf1_on();
        delay(500000U);
        pf1_off();
        delay(500000U);
    }
}
