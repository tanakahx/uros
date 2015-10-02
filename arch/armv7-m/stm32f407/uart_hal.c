#include "stm32f4xx.h"
#include "system.h"
#include "uart_hal.h"

static USART_TypeDef *const uart[] = {USART1, USART2, USART3, UART4, UART5};
static const IRQn_Type irq[] = {USART1_IRQn, USART2_IRQn, USART3_IRQn, UART4_IRQn, UART4_IRQn};

#define NR_UART (sizeof(uart)/sizeof(uart[0]))

typedef struct {
    bool_t send_cbr_en;
    bool_t recv_cbr_en;
    void (*send_cbr)(void);
    void (*recv_cbr)(void);
} uart_hal_t;

uart_hal_t uart_hal[NR_UART];

int uart_hal_init(uint32_t devno)
{
    int i;

    if (devno >= NR_UART)
        return 1;

    for (i = 0; i < NR_UART; i++) {
        uart_hal[i].send_cbr_en = FALSE;
        uart_hal[i].recv_cbr_en = FALSE;
        uart_hal[i].send_cbr = NULL;
        uart_hal[i].recv_cbr = NULL;
    }

    return 0;
}

int uart_hal_open(uint32_t devno, const uart_hal_oinfo_t *info)
{
    if (devno >= NR_UART)
        return 1;

    GPIOD->MODER |= GPIO_MODER_MODER5_1 | GPIO_MODER_MODER6_1;
    GPIOD->AFR[0] = (0x7 << (5*4)) | (0x7 << (6*4));

    uart[devno]->CR1 |=
        USART_CR1_UE |    /* USART enable */
        USART_CR1_TE |    /* Transmitter enable */
        USART_CR1_RE;     /* Receiver enable */

    /* A real hardware requires baud rate settings. */
    uart[devno]->BRR = 0x16D; /* PCLK = 84MHz, Baud rate = 115200 bps, OVER8 = 0, hence BRR = 22.8125 */

    nvic_set_irq_pri(irq[devno], info->pri);

    uart_hal[devno].send_cbr = info->send_cbr;
    uart_hal[devno].recv_cbr = info->recv_cbr;

    return 0;
}

int uart_hal_close(uint32_t devno)
{
    if (devno >= NR_UART)
        return 1;

    uart[devno]->CR1 &=
        ~(USART_CR1_UE |
          USART_CR1_TE |
          USART_CR1_RE);

    uart_hal[devno].send_cbr_en = FALSE;
    uart_hal[devno].recv_cbr_en = FALSE;
    uart_hal[devno].send_cbr = NULL;
    uart_hal[devno].recv_cbr = NULL;

    return 0;
}

int uart_hal_enable_cbr(uint32_t devno, uart_hal_cbr_flag_t flag)
{
    if (devno >= NR_UART)
        return 1;

    if (flag == UART_HAL_CBR_FLAG_SEND) {
        uart_hal[devno].send_cbr_en = TRUE;
    }

    if (flag == UART_HAL_CBR_FLAG_RECV) {
        uart[devno]->CR1 |= USART_CR1_RXNEIE; /* RXNE interrupt enable */
        uart_hal[devno].recv_cbr_en = TRUE;
    }

    /* Enable external IRQ */
    nvic_enable_irq(irq[devno]);

    return 0;
}

size_t uart_hal_send(uint32_t devno, char c)
{
    if (devno >= NR_UART)
        return 0;

    if (!(uart[devno]->SR & USART_SR_TXE))
        return 0;

    uart[devno]->DR = c;

    if (uart_hal[devno].send_cbr_en)
        uart[devno]->CR1 |= USART_CR1_TXEIE;

    return 1;
}

size_t uart_hal_recv(uint32_t devno, char *c)
{
    if (devno >= NR_UART)
        return 0;

    if (!(uart[devno]->SR & USART_SR_RXNE)) /* when receive FIFO is empty */
        return 0;

    *c = uart[devno]->DR;

    return 1;
}

void USART2_IRQHandler()
{
    if (uart[1]->SR & USART_SR_TXE) {
        uart[1]->CR1 &= ~USART_CR1_TXEIE;
        if (uart_hal[1].send_cbr_en)
            uart_hal[1].send_cbr();
    }

    if (uart[1]->SR & USART_SR_RXNE) {
        if (uart_hal[1].recv_cbr_en)
            uart_hal[1].recv_cbr();
    }
}
