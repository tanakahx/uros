#include "system.h"
#include "uros.h"
#include "lib.h"

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

void pend_sv(void)
{
    ICSR |= (1<<28);
}

void initialize_system(void)
{
    extern uint32_t rodata_end;
    extern uint32_t data_start;
    extern uint32_t data_end;
    
    /* Disable all interrupts until initialization is completed. */
    disable_interrupt();
    
    /* Clear SRAM area with zero (bss section is cleared here) */
    memset((void *)LOAD_ADDR, 0, SRAM_SIZE);
    
    /* Copy .data section into SRAM area */
    memcpy((void *)LOAD_ADDR, (void *)&rodata_end, (void *)&data_end - (void *)&data_start);
    
    /* Priority level less than or equal to 0 is allowed when interrupt is enabled. */
    set_basepri(0);
    
    /* Set priority group to 0 */
    AIRCR = 0x05FA0000;

    /* Set system exception priority */
    SHPR1 = 0x00000000;
    SHPR2 = 0x00000000;
    SHPR3 = 0x01010000;

    /* Copy vector table from ROM to RAM */
    memcpy((void *)VECTOR_ADDR, (void *)0, VECTOR_SIZE);
    VTOR = VECTOR_ADDR;
    
    /* TODO: Enable external IRQ */


    /* Enable double word stack alignment */
    NVIC_CCR |= 0x200;
}
