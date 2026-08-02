#ifndef TM4C123_REGISTERS_H
#define TM4C123_REGISTERS_H

#include <stdint.h>

#define HWREG32(address) \
    (*((volatile uint32_t *)(address)))

#define BITBAND_PERIPHERAL(address, bit) \
    (*(volatile uint32_t *)( \
        0x42000000U + \
        (((uint32_t)(address) - 0x40000000U) * 32U) + \
        ((uint32_t)(bit) * 4U)))

/* System Control */
#define SYSCTL_RCGCGPIO_R HWREG32(0x400FE608U)
#define SYSCTL_PRGPIO_R   HWREG32(0x400FEA08U)
#define SYSCTL_RCGCUART_R HWREG32(0x400FE618U)
#define SYSCTL_PRUART_R   HWREG32(0x400FEA18U)

/* GPIO Port A */
#define GPIO_PORTA_AFSEL_R HWREG32(0x40004420U)
#define GPIO_PORTA_DEN_R   HWREG32(0x4000451CU)
#define GPIO_PORTA_PCTL_R  HWREG32(0x4000452CU)
#define GPIO_PORTA_AMSEL_R HWREG32(0x40004528U)

/* GPIO Port B */
#define GPIO_PORTB_DATA_R  HWREG32(0x400053FCU)
#define GPIO_PORTB_DIR_R   HWREG32(0x40005400U)
#define GPIO_PORTB_AFSEL_R HWREG32(0x40005420U)
#define GPIO_PORTB_ODR_R   HWREG32(0x4000550CU)
#define GPIO_PORTB_DEN_R   HWREG32(0x4000551CU)
#define GPIO_PORTB_AMSEL_R HWREG32(0x40005528U)
#define GPIO_PORTB_PCTL_R  HWREG32(0x4000552CU)

/* GPIO Port F */
#define GPIO_PORTF_DATA_R_RAW 0x400253FCU
#define GPIO_PORTF_DATA_R     HWREG32(GPIO_PORTF_DATA_R_RAW)
#define GPIO_PORTF_DIR_R      HWREG32(0x40025400U)
#define GPIO_PORTF_DEN_R      HWREG32(0x4002551CU)

/* UART0 */
#define UART0_DR_R   HWREG32(0x4000C000U)
#define UART0_FR_R   HWREG32(0x4000C018U)
#define UART0_IBRD_R HWREG32(0x4000C024U)
#define UART0_FBRD_R HWREG32(0x4000C028U)
#define UART0_LCRH_R HWREG32(0x4000C02CU)
#define UART0_CTL_R  HWREG32(0x4000C030U)
#define UART0_CC_R   HWREG32(0x4000CFC8U)

/* Common bit masks */
#define GPIO_PORTA_CLOCK_BIT (1U << 0)
#define GPIO_PORTB_CLOCK_BIT (1U << 1)
#define GPIO_PORTF_CLOCK_BIT (1U << 5)
#define UART0_CLOCK_BIT      (1U << 0)

#define PA1_BIT              (1U << 1)
#define PB0_PIN              0U
#define PB0_BIT              (1U << PB0_PIN)

#define PB1_PIN              1U
#define PB1_BIT              (1U << PB1_PIN)

#define PF1_PIN              1U
#define PF1_BIT              (1U << PF1_PIN)
#define UART_FR_TXFF         (1U << 5)

#define UART_CTL_UARTEN      (1U << 0)
#define UART_CTL_TXE         (1U << 8)
#endif
