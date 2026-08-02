.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb


.extern main
.extern _estack
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss


/* System Control Block registers */
.equ SCB_CPACR,      0xE000ED88
.equ SCB_FPCCR,      0xE000EF34

/* CPACR bits for CP10 and CP11 full access */
.equ CPACR_CP10_CP11_FULL_ACCESS, 0x00F00000

/* Optional floating-point context-control bits */
.equ FPCCR_ASPEN,    (1 << 31)
.equ FPCCR_LSPEN,    (1 << 30)


.section .isr_vector, "a", %progbits
.align 2
.global vector_table
.type vector_table, %object

vector_table:
    .word _estack
    .word Reset_Handler
    .word Default_Handler       /* NMI */
    .word Default_Handler       /* HardFault */
    .word Default_Handler       /* MemManage */
    .word Default_Handler       /* BusFault */
    .word Default_Handler       /* UsageFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler       /* SVCall */
    .word Default_Handler       /* Debug Monitor */
    .word 0
    .word Default_Handler       /* PendSV */
    .word Default_Handler       /* SysTick */

.size vector_table, . - vector_table


.section .text.Reset_Handler, "ax", %progbits
.align 2
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func

Reset_Handler:

    /*
     * Enable the Cortex-M4F floating-point unit.
     *
     * CP10 is controlled by CPACR bits 21:20.
     * CP11 is controlled by CPACR bits 23:22.
     *
     * Setting both fields to 0b11 gives full access.
     */
    ldr r0, =SCB_CPACR
    ldr r1, [r0]
    ldr r2, =CPACR_CP10_CP11_FULL_ACCESS
    orr r1, r1, r2
    str r1, [r0]

    /*
     * Ensure the CPACR write is complete before any
     * floating-point instruction is executed.
     */
    dsb
    isb

    /*
     * Optional: enable automatic and lazy preservation of the
     * floating-point context during exception entry.
     *
     * This is useful later when interrupts or an RTOS use
     * floating-point instructions.
     */
    ldr r0, =SCB_FPCCR
    ldr r1, [r0]
    ldr r2, =(FPCCR_ASPEN | FPCCR_LSPEN)
    orr r1, r1, r2
    str r1, [r0]

    /*
     * Copy initialized .data variables from Flash to SRAM.
     *
     * r0 = source address in Flash
     * r1 = destination address in SRAM
     * r2 = end of .data in SRAM
     */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

copy_data:
    cmp r1, r2
    bcs clear_bss

    ldr r3, [r0], #4
    str r3, [r1], #4
    b copy_data


clear_bss:
    /*
     * Clear all zero-initialized variables in .bss.
     *
     * r1 = start of .bss
     * r2 = end of .bss
     * r3 = zero
     */
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0

clear_bss_loop:
    cmp r1, r2
    bcs call_main

    str r3, [r1], #4
    b clear_bss_loop


call_main:
    bl main


hang:
    b hang

.size Reset_Handler, . - Reset_Handler


.section .text.Default_Handler, "ax", %progbits
.align 2
.global Default_Handler
.type Default_Handler, %function
.thumb_func

Default_Handler:
    b Default_Handler

.size Default_Handler, . - Default_Handler