#include "system.h"
#include "lib.h"
#include "uros.h"
#include "uart_hal.h"

extern void reset_handler(void);
extern void system_init(void);

__attribute__((naked))
void disable_interrupt(void)
{
    asm volatile("mov r0, #1;"
                 "msr PRIMASK, r0;"
                 "bx lr;");
}

__attribute__((naked))
void enable_interrupt(void)
{
    asm volatile("mov r0, #0;"
                 "msr PRIMASK, r0;"
                 "bx lr;");
}

__attribute__((naked))
void set_basepri(int val)
{
    asm volatile("msr BASEPRI, r0;"
                 "bx lr;");
}

__attribute__((naked))
void set_psp(uint32_t *val)
{
    asm volatile("msr PSP, r0;"
                 "bx lr;");
}

void pend_sv(void)
{
    ICSR |= (1<<28);
}

void memory_init()
{
    extern unsigned int rodata_end;
    extern unsigned int data_start;
    extern unsigned int data_end;
    extern unsigned int sram_start;
    extern unsigned int sram_end;

    /* Clear SRAM area with zero (bss section is cleared here) */
    memset((void *)&sram_start, 0, &sram_end - &sram_start);

    /* Copy .data section into SRAM area */
    memcpy((void *)&sram_start, (void *)&rodata_end, (void *)&data_end - (void *)&data_start);
}

void Reset_Handler(void)
{
    /* System dependent initialization */
    system_init();

    /* Disable all interrupts until initialization is completed. */
    disable_interrupt();

    memory_init();

    /* Priority level less than or equal to 0 is allowed when interrupt is enabled. */
    set_basepri(0);

    /* Set priority group to 0 */
    AIRCR = 0x05FA0000;

    /* Set system exception priority */
    SHPR1 = 0x00000000;
    SHPR2 = 0x00000000;
    SHPR3 = 0x01010000;

    /* TODO: Enable external IRQ */


    /* Enable double word stack alignment */
    NVIC_CCR |= 0x200;

    /* Let's get started. */
    uart_hal_send('b');
    uart_hal_send('o');
    uart_hal_send('o');
    uart_hal_send('t');
    uart_hal_send('i');
    uart_hal_send('n');
    uart_hal_send('g');
    uart_hal_send('.');
    uart_hal_send('.');
    uart_hal_send('.');
    uart_hal_send('\r');
    uart_hal_send('\n');
    uros_main();
}
