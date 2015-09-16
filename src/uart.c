#include "stdtype.h"
#include "uros.h"
#include "system.h"
#include "uart_hal.h"
#include "uart.h"
#include "config.h"

status_type_t sys_set_event(task_type_t task_id, event_mask_type_t event);

static uart_que_t req_que;
static bool_t uart_initialized[] = {FALSE, FALSE, FALSE};

void uart_alarm_callback(void)
{
    uart_que_t *send_q = req_que.send_prev;
    uart_que_t *recv_q = req_que.recv_prev;

    while (send_q != &req_que) {
        if (send_q->timeout && (send_q->over-- == 0)) {
            disable_interrupt();
            send_q->send_next->send_prev = send_q->send_prev;
            send_q->send_prev->send_next = send_q->send_next;
            enable_interrupt();
            sys_set_event(send_q->task_id, EV_UART_TIMEOUT);
        }
        send_q = send_q->send_prev;
    }
    while (recv_q != &req_que) {
        if (recv_q->timeout && (recv_q->over-- == 0)) {
            disable_interrupt();
            recv_q->recv_next->recv_prev = recv_q->recv_prev;
            recv_q->recv_prev->recv_next = recv_q->recv_next;
            enable_interrupt();
            sys_set_event(recv_q->task_id, EV_UART_TIMEOUT);
        }
        recv_q = recv_q->recv_prev;
    }
}

void uart_send_cbr(void)
{
    uart_que_t *q;
    bool_t wakeup = FALSE;
    
    disable_interrupt();
    if (req_que.send_prev != &req_que) {
        q = req_que.send_prev;
        if (--q->size == 0) {
            q->send_next->send_prev = q->send_prev;
            q->send_prev->send_next = q->send_next;
            wakeup = TRUE;
        }
        else
            uart_hal_send(q->devno, *++q->buf);
    }
    enable_interrupt();
    
    if (wakeup)
        sys_set_event(q->task_id, EV_UART_COMPLETE);
}

void uart_recv_cbr(void)
{
    uart_que_t *q;
    bool_t wakeup = FALSE;

    disable_interrupt();
    if (req_que.recv_prev != &req_que) {
        q = req_que.recv_prev;
        uart_hal_recv(q->devno, q->buf++);
        if (--q->size == 0) {
            q->recv_next->recv_prev = q->recv_prev;
            q->recv_prev->recv_next = q->recv_next;
            wakeup = TRUE;
        }
    }
    enable_interrupt();

    if (wakeup)
        sys_set_event(q->task_id, EV_UART_COMPLETE);
}

int uart_open(uart_info_t *dev)
{
    uart_hal_oinfo_t oinfo;
    
    oinfo.baud_rate = dev->baud_rate;
    oinfo.pri = 1;
    oinfo.send_cbr = uart_send_cbr;
    oinfo.recv_cbr = uart_recv_cbr;

    uart_hal_open(dev->devno, &oinfo);
    uart_hal_enable_cbr(dev->devno, UART_HAL_CBR_FLAG_SEND);
    uart_hal_enable_cbr(dev->devno, UART_HAL_CBR_FLAG_RECV);

    disable_interrupt();
    if (uart_initialized[dev->devno] == FALSE) {
        req_que.send_next = &req_que;
        req_que.send_prev = &req_que;
        req_que.recv_next = &req_que;
        req_que.recv_prev = &req_que;
        uart_initialized[dev->devno] = TRUE;
    }
    enable_interrupt();
    
    set_abs_alarm(UART_ALARM, 0, 1);

    return 0;
}

int uart_close(uart_info_t *dev)
{
    return uart_hal_close(dev->devno);
}

task_type_t task_id(void)
{
    task_type_t id;

    /* internal use of system call */
    sys_get_task_id(&id);
    return id;
}

void uart_write_request(uint32_t devno, char *buf, size_t size, uart_que_t *que)
{
    que->devno = devno;
    que->buf = buf;
    que->size = size;
    que->task_id = task_id();

    disable_interrupt();
    /* enqueue */
    que->send_next = req_que.send_next;
    que->send_prev = &req_que;
    req_que.send_next->send_prev = que;
    req_que.send_next = que;
    uart_hal_send(devno, *buf);
    enable_interrupt();
}

int uart_write(uint32_t devno, char *buf, size_t size)
{
    return uart_twrite(devno, buf, size, 0);
}

int uart_twrite(uint32_t devno, char *buf, size_t size, tick_t over)
{
    uart_que_t que;
    event_mask_type_t event;

    if (over > 0)
        que.timeout = TRUE;
    else
        que.timeout = FALSE;
    que.over = over;
    uart_write_request(devno, buf, size, &que);

    if (over > 0)
        wait_event(EV_UART_COMPLETE | EV_UART_TIMEOUT);
    else
        wait_event(EV_UART_COMPLETE);

    get_event(task_id(), &event);
    clear_event(event);

    if (over > 0) {
        if (event & EV_UART_TIMEOUT)
            return -1;
    }
    else if (!(event & EV_UART_COMPLETE))
        return -2;

    return size - que.size;
}

void uart_read_request(uint32_t devno, char *buf, size_t size, uart_que_t *que)
{
    que->devno = devno;
    que->buf = buf;
    que->size = size;
    que->task_id = task_id();

    disable_interrupt();
    /* enqueue */
    que->recv_next = req_que.recv_next;
    que->recv_prev = &req_que;
    req_que.recv_next->recv_prev = que;
    req_que.recv_next = que;
    enable_interrupt();
}

int uart_read(uint32_t devno, char *buf, size_t size)
{
    return uart_tread(devno, buf, size, 0);
}

int uart_tread(uint32_t devno, char *buf, size_t size, tick_t over)
{
    uart_que_t que;
    event_mask_type_t event;

    if (over > 0)
        que.timeout = TRUE;
    else
        que.timeout = FALSE;
    que.over = over;
    uart_read_request(devno, buf, size, &que);

    if (over > 0)
        wait_event(EV_UART_COMPLETE | EV_UART_TIMEOUT);
    else
        wait_event(EV_UART_COMPLETE);

    get_event(task_id(), &event);
    clear_event(event);

    if (over > 0) {
        if ((event & EV_UART_TIMEOUT))
            return -1;
    }
    else if (!(event & EV_UART_COMPLETE))
        return -2;

    return size - que.size;
}
