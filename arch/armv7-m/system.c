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

void nvic_enable_irq(uint32_t irq)
{
    NVIC_ISER[irq >> 5] = 0x1 << (irq & 0x1F);
}

void nvic_set_irq_pri(uint32_t irq, uint32_t pri)
{
    NVIC_IPR[irq] = pri;
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
    /* Disable all interrupts until initialization is completed. */
    disable_interrupt();

    /* System dependent initialization */
    system_init();

    memory_init();

    /* Priority level less than or equal to 0 is allowed when interrupt is enabled. */
    set_basepri(0);

    /* Set priority group to 0 */
    AIRCR = 0x05FA0000;

    /* Set system exception priority */
    SHPR1 = 0x00000000;
    SHPR2 = 0x00000000;
    SHPR3 = 0x02020000;

    /* Enable double word stack alignment */
    NVIC_CCR |= 0x200;

    uros_main();
}
