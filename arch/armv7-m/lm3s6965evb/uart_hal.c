#include "lm3s6965evb.h"
#include "system.h"
#include "uart_hal.h"

static uart_t *uart[] = {UART0, UART1, UART2};
static uint32_t irq[] = {5, 6, 33};
static void (*uart_send_cbr)(void) = NULL;
static void (*uart_recv_cbr)(void) = NULL;

#define NR_UART (sizeof(uart)/sizeof(uart[0]))

int uart_hal_init(uint32_t devno)
{
    if (devno >= NR_UART)
        return 1;

    uart[devno]->CTL |= 0x301;

    return 0;
}

int uart_hal_open(uint32_t devno, const uart_hal_oinfo_t *info)
{
    if (devno >= NR_UART)
        return 1;

    /* A real hardware requires baud rate settings. */
    /* UART0->IBRD = XXX; */
    /* UART0->FBRD = YYY; */

    nvic_set_irq_pri(irq[devno], info->pri);

    uart_send_cbr = info->send_cbr;
    uart_recv_cbr = info->recv_cbr;

    return 0;
}

int uart_hal_close(uint32_t devno)
{
    if (devno >= NR_UART)
        return 1;

    uart[devno]->CTL &= ~0x301;

    return 0;
}

int uart_hal_enable_cbr(uint32_t devno, uart_hal_cbr_flag_t flag)
{
    if (devno >= NR_UART)
        return 1;

    uart[devno]->ICR = 0xFF;

    if (flag == UART_HAL_CBR_FLAG_SEND)
        uart[devno]->IM |= 0x1 << 5;

    if (flag == UART_HAL_CBR_FLAG_RECV)
        uart[devno]->IM |= 0x1 << 4;

    /* Enable external IRQ */
    nvic_enable_irq(irq[devno]);

    return 0;
}

size_t uart_hal_send(uint32_t devno, char c)
{
    if (devno >= NR_UART)
        return 0;

    if ((uart[devno]->FR & 0x8) != 0) /* when data is being transmitted */
        return 0;

    uart[devno]->DR = c;

    return 1;
}

size_t uart_hal_recv(uint32_t devno, char *c)
{
    if (devno >= NR_UART)
        return 0;

    if ((uart[devno]->FR & 0x10) != 0) /* when receive FIFO is empty */
        return 0;

    *c = uart[devno]->DR;

    return 1;
}

void Uart0_Handler()
{
    if (uart[0]->MIS & (0x1 << 5)) {
        uart[0]->ICR = 0x1 << 5;
        if (uart_send_cbr)
            uart_send_cbr();
    }

    if (uart[0]->MIS & (0x1 << 4)) {
        uart[0]->ICR = 0x1 << 4;
        if (uart_recv_cbr)
            uart_recv_cbr();
    }
}
