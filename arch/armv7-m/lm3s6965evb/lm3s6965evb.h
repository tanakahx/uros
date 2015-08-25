#ifndef LM3S6965EVB_H
#define LM3S6965EVB_H

#include "stdtype.h"

#define UART0_BASE 0x4000C000
#define UART0 ((uart_t *)UART0_BASE)

typedef struct uart {
    uint32_t DR;
    uint32_t RSR;
    uint32_t dummy[4];
    uint32_t FR;
} uart_t;

#endif
