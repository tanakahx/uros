#ifndef UART_HAL_H
#define UART_HAL_H

#include "stdtype.h"

typedef struct {
    uint32_t baud_rate;
    uint32_t pri;
    void (*send_cbr)();
    void (*recv_cbr)();
} uart_hal_oinfo_t;

typedef enum {
    UART_HAL_CBR_FLAG_SEND,
    UART_HAL_CBR_FLAG_RECV
} uart_hal_cbr_flag_t;

int uart_hal_init(uint32_t devno);
int uart_hal_open(uint32_t devno, const uart_hal_oinfo_t *info);
int uart_hal_close(uint32_t devno);
size_t uart_hal_send(uint32_t devno, char c);
size_t uart_hal_recv(uint32_t devno, char *c);
int uart_hal_enable_cbr(uint32_t devno, uart_hal_cbr_flag_t flag);

#endif
