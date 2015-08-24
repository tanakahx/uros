#include "stm32f4xx.h"

int uart_hal_send(char c)
{
    while (!(USART2->SR & USART_SR_TXE))
        continue;

    USART2->DR = c;

    return 0;
}

int uart_hal_recv(char *c)
{
    *c = 0x41; /* DUMMY */

    return 0;
}
