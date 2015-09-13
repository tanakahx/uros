#ifndef __UART_H__
#define __UART_H__

#include "system.h"
#include "uros.h"

typedef struct uart_que {
    struct uart_que *send_next;
    struct uart_que *send_prev;
    struct uart_que *recv_next;
    struct uart_que *recv_prev;
    uint32_t devno;
    char *buf;
    size_t size;
    bool_t timeout;
    tick_t over;
    task_type_t task_id;
} uart_que_t;

typedef struct {
    uint32_t devno;
    uint32_t baud_rate;
} uart_info_t;

int uart_open(uart_info_t *dev);
int uart_close(uart_info_t *dev);
int uart_write(uint32_t devno, char *buf, size_t size);
int uart_twrite(uint32_t devno, char *buf, size_t size, tick_t over);
int uart_read(uint32_t devno, char *buf, size_t size);
int uart_tread(uint32_t devno, char *buf, size_t size, tick_t over);

#endif
