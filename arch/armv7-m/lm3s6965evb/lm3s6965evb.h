#ifndef LM3S6965EVB_H
#define LM3S6965EVB_H

#include "stdtype.h"

#define UART0_BASE 0x4000C000
#define UART1_BASE 0x4000D000
#define UART2_BASE 0x4000E000

#define UART0 ((uart_t *)UART0_BASE)
#define UART1 ((uart_t *)UART1_BASE)
#define UART2 ((uart_t *)UART2_BASE)

typedef struct uart {
    uint32_t DR;
    uint32_t RSR;
    uint32_t dummy0[4];
    uint32_t FR;
    uint32_t dummy1;
    uint32_t ILPR;
    uint32_t IBRD;
    uint32_t FBRD;
    uint32_t LCRH;
    uint32_t CTL;
    uint32_t IFLS;
    uint32_t IM;
    uint32_t RIS;
    uint32_t MIS;
    uint32_t ICR;
} uart_t;

#endif
