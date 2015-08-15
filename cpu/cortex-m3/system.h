#ifndef SYSTEM_H
#define SYSTEM_H

#include "uros.h"

#define SRAM_ADDR 0x20000000
#define SRAM_SIZE 0x00010000
#define STACK_BTM (SRAM_ADDR + SRAM_SIZE)

#define NR_IRQ 64

#define VECTOR_ADDR SRAM_ADDR
#define VECTOR_SIZE (4 * NR_IRQ)
#define LOAD_ADDR   (VECTOR_ADDR + VECTOR_SIZE)

#define UART0_ADDR 0x4000C000
#define NVIC_ADDR  0xE000E000
#define ICSR       (*(volatile uint32_t *)0xE000ED04)
#define VTOR       (*(volatile uint32_t *)0xE000ED08)
#define AIRCR      (*(volatile uint32_t *)0xE000ED0C)
#define NVIC_CCR   (*(volatile uint32_t *)0xE000ED14)

#define SHPR1      (*(volatile uint32_t *)0xE000ED18)
#define SHPR2      (*(volatile uint32_t *)0xE000ED1C)
#define SHPR3      (*(volatile uint32_t *)0xE000ED20)

#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)
#define SYST_CALIB (*(volatile uint32_t *)0xE000E01C)
#define SYST_CSR_TICKINT 0x2

void disable_interrupt(void);
void enable_interrupt(void);
void set_basepri(int val);
void pend_sv(void);
void initialize_system(void);

#endif
