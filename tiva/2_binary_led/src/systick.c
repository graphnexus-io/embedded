#include "systick.h"
#include "tm4c123_registers.h"

#define SYSTICK_TICKS_PER_MS 16000U

static void systick_wait_ticks(uint32_t ticks)
{
    NVIC_ST_RELOAD_R = ticks - 1U;
    NVIC_ST_CURRENT_R = 0U;

    while ((NVIC_ST_CTRL_R & SYSTICK_COUNT) == 0U) {
    }
}

void systick_init(void)
{
    NVIC_ST_CTRL_R = 0U;
    NVIC_ST_RELOAD_R = 0x00FFFFFFU;
    NVIC_ST_CURRENT_R = 0U;
    NVIC_ST_CTRL_R = SYSTICK_CLK_SRC | SYSTICK_ENABLE;
}

void systick_wait_ms(uint32_t milliseconds)
{
    for (uint32_t elapsed = 0U; elapsed < milliseconds; ++elapsed) {
        systick_wait_ticks(SYSTICK_TICKS_PER_MS);
    }
}
