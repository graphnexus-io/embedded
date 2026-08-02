#include "uart0.h"
#include "tm4c123_registers.h"

void uart0_init(void)
{
    SYSCTL_RCGCUART_R |= UART0_CLOCK_BIT;

    while ((SYSCTL_PRUART_R & UART0_CLOCK_BIT) == 0U) {
    }

    SYSCTL_RCGCGPIO_R |= GPIO_PORTA_CLOCK_BIT;

    while ((SYSCTL_PRGPIO_R & GPIO_PORTA_CLOCK_BIT) == 0U) {
    }

    GPIO_PORTA_AFSEL_R |= PA1_BIT;

    GPIO_PORTA_PCTL_R &= ~0x000000F0U;
    GPIO_PORTA_PCTL_R |=  0x00000010U;

    GPIO_PORTA_DEN_R |= PA1_BIT;
    GPIO_PORTA_AMSEL_R &= ~PA1_BIT;

    UART0_CTL_R &= ~UART_CTL_UARTEN;

    UART0_CC_R = 0U;

    UART0_IBRD_R = 8U;
    UART0_FBRD_R = 44U;

    UART0_LCRH_R =
        (3U << 5) |
        (1U << 4);

    UART0_CTL_R =
        UART_CTL_UARTEN |
        UART_CTL_TXE;
}

void uart0_putc(char c)
{
    while ((UART0_FR_R & UART_FR_TXFF) != 0U) {
    }

    UART0_DR_R = (uint32_t)c;
}

void uart0_puts(const char *text)
{
    while (*text != '\0') {
        uart0_putc(*text++);
    }
}
