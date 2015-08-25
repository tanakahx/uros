#include "lm3s6965evb.h"

int uart_hal_send(char c)
{
    UART0->DR = c;

    return 0;
}

int uart_hal_recv(char *c)
{
    while ((UART0->FR & 0x10) != 0)
        continue;

    *c = UART0->DR;

    return 0;
}
