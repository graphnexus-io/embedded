#include "gpio.h"
#include "tm4c123_registers.h"

void gpio_portf_init(void)
{

    SYSCTL_RCGCGPIO_R |= GPIO_PORTF_CLOCK_BIT;

    while ((SYSCTL_PRGPIO_R & GPIO_PORTF_CLOCK_BIT) == 0U) {
    }

    GPIO_PORTF_DIR_R |= PF1_BIT;
    GPIO_PORTF_DEN_R |= PF1_BIT;
}

void gpio_portb_pb0_init(void)
{
    /* Enable clock for GPIO Port B. */
    SYSCTL_RCGCGPIO_R |= GPIO_PORTB_CLOCK_BIT;

    /* Wait until Port B is ready. */
    while ((SYSCTL_PRGPIO_R & GPIO_PORTB_CLOCK_BIT) == 0U) {
    }

    /* Disable analog mode on PB0. */
    GPIO_PORTB_AMSEL_R &= ~PB0_BIT;

    /* Select ordinary GPIO instead of an alternate peripheral. */
    GPIO_PORTB_AFSEL_R &= ~PB0_BIT;

    /*
     * Clear PB0's four-bit PCTL field.
     * PB0 uses bits [3:0].
     */
    GPIO_PORTB_PCTL_R &= ~0x0000000FU;

    /* Select normal push-pull operation. */
    GPIO_PORTB_ODR_R &= ~PB0_BIT;

    /* Set the output latch low before enabling the output driver. */
    GPIO_PORTB_DATA_R &= ~PB0_BIT;

    /* Configure PB0 as an output. */
    GPIO_PORTB_DIR_R |= PB0_BIT;

    /* Enable the digital function on PB0. */
    GPIO_PORTB_DEN_R |= PB0_BIT;
}


void gpio_portb_pb1_init(void)
{
    /* Enable clock for GPIO Port B. */
    SYSCTL_RCGCGPIO_R |= GPIO_PORTB_CLOCK_BIT;

    /* Wait until Port B is ready. */
    while ((SYSCTL_PRGPIO_R & GPIO_PORTB_CLOCK_BIT) == 0U) {
    }

    /* Disable analog mode on PB1. */
    GPIO_PORTB_AMSEL_R &= ~PB1_BIT;

    /* Select ordinary GPIO instead of an alternate peripheral. */
    GPIO_PORTB_AFSEL_R &= ~PB1_BIT;

    /*
     * Clear PB1's four-bit PCTL field.
     * PB1 uses bits [7:4].
     */
    GPIO_PORTB_PCTL_R &= ~0x000000F0U;

    /* Configure PB1 as an output. */
    GPIO_PORTB_DIR_R |= PB1_BIT;

    /* Enable the digital function on PB1. */
    GPIO_PORTB_DEN_R |= PB1_BIT;

    /* Start with off. */
    GPIO_PORTB_DATA_R &= ~PB1_BIT;
}


void apf1_on(void) {
    BITBAND_PERIPHERAL(GPIO_PORTF_DATA_R_RAW, PF1_PIN) = 1U;
}

void apf1_off(void) {
    BITBAND_PERIPHERAL(GPIO_PORTF_DATA_R_RAW, PF1_PIN) = 0U;
}

void pf1_on(void)
{
    GPIO_PORTF_DATA_R |= PF1_BIT;
}

void pf1_off(void)
{
    GPIO_PORTF_DATA_R &= ~PF1_BIT;
}

void pf1_toggle(void)
{
    GPIO_PORTF_DATA_R ^= PF1_BIT;
}

void pb0_on(void)
{
    GPIO_PORTB_DATA_R |= PB0_BIT;
}

void pb0_off(void)
{
    GPIO_PORTB_DATA_R &= ~PB0_BIT;
}

void pb1_on(void)
{
    GPIO_PORTB_DATA_R |= PB1_BIT;
}

void pb1_off(void)
{
    GPIO_PORTB_DATA_R &= ~PB1_BIT;
}





void pb0_toggle(void)
{
    GPIO_PORTB_DATA_R ^= PB0_BIT;
}
