#ifndef SYSTEM_H
#define SYSTEM_H

#define SRAM_ADDR 0x20000000
#define SRAM_SIZE 0x00010000

#define UART0_ADDR 0x4000C000
#define NVIC_ADDR  0xE000E000
#define NVIC_CCR   (*(volatile unsigned int *)0xE000ED14)

#endif
