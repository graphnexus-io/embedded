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
    /* Copy initialized .data from Flash to SRAM. */
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
    /* Clear zero-initialized .bss. */
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
