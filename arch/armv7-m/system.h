#ifndef SYSTEM_H
#define SYSTEM_H

#ifdef LM3S6965EVB
#include "lm3s6965evb.h"
#define BUILD_TARGET_ARCH "LM3S6965EVB"
#endif

#ifdef STM32F407xx
#include "stm32f4xx.h"
#define BUILD_TARGET_ARCH "STM32F407"
#endif

#include "stdtype.h"

#define NVIC_ADDR  0xE000E000
#define ICSR       (*(volatile uint32_t *)0xE000ED04)
#define VTOR       (*(volatile uint32_t *)0xE000ED08)
#define AIRCR      (*(volatile uint32_t *)0xE000ED0C)
#define NVIC_CCR   (*(volatile uint32_t *)0xE000ED14)
#define NVIC_ISER  ( (volatile uint32_t *)0xE000E100)
#define NVIC_IPR   ( (volatile uint32_t *)0xE000E400)

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
void set_psp(uint32_t *val);
void pend_sv(void);
void nvic_enable_irq(uint32_t irq);
void nvic_set_irq_pri(uint32_t irq, uint32_t pri);

#endif
