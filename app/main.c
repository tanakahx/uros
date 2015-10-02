#include "uros.h"
#include "uart.h"
#include "lib.h"
#include "config.h"
#include "uart_hal.h"

void sub_task1(int ex)
{
    uint32_t ev;

    get_resource(RESOURCE1);
    puts("[sub_task1]: start");
    release_resource(RESOURCE1);
    wait_event(EVENT1);
    get_event(SUB_TASK1, &ev);
    if (ev & EVENT1)
        puts("[sub_task1]: wake up by main_task");
    else
        puts("[sub_task1]: wake up by unknown");
    clear_event(-1);
    puts("[sub_task1]: done");
    terminate_task();
}

void sub_task2(int ex)
{
    get_resource(RESOURCE1);
    puts("[sub_task2]: start");
    puts("[sub_task2]: done");
    release_resource(RESOURCE1);
    terminate_task();
}

void main_task_callback(void)
{
    extern status_type_t sys_activate_task(task_type_t task_id);
    sys_activate_task(SUB_TASK2);
}

void main_task(int ex)
{
    volatile int i = 0;
    char buf[64];

    gets(buf);
    puts(buf);
    puts("[main_task]: start");

    /* Start them */
    activate_task(SUB_TASK1);
    activate_task(SUB_TASK2);

    /* Set alarm */
    set_rel_alarm(ALARM3, 0, 200);

    get_resource(RESOURCE1);
    puts("[main_task]: set_event");
    release_resource(RESOURCE1);
    set_event(SUB_TASK1, EVENT1);

    while (1) {
        if (i++ == 0x80000) {
            get_resource(RESOURCE1);
            puts("[main_task]");
            release_resource(RESOURCE1);
            i = 0;
        }
    }
}

void send1(uint32_t devno, char c)
{
    while (!uart_hal_send(devno, c))
        continue;
}

int main()
{
    uart_hal_oinfo_t info;
    int devno;

#ifdef LM3S6965EVB
    devno = 0;
#elif  STM32F407xx
    devno = 1;
#endif
    info.pri = 1;
    uart_hal_init(devno);
    uart_hal_open(devno, &info);
    send1(devno, 'b');
    send1(devno, 'o');
    send1(devno, 'o');
    send1(devno, 't');
    send1(devno, 'i');
    send1(devno, 'n');
    send1(devno, 'g');
    send1(devno, '.');
    send1(devno, '.');
    send1(devno, '.');
    send1(devno, '\r');
    send1(devno, '\n');

    start_os();

    return 0;
}
