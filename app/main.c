#include "uros.h"
#include "uart.h"
#include "lib.h"
#include "config.h"

void sub_task1(int ex)
{
    uint32_t ev;

    puts("[sub_task1]: start");
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

    puts("[main_task]: start");

    /* Start them */
    activate_task(SUB_TASK1);
    activate_task(SUB_TASK2);

    /* Set alarm */
    set_rel_alarm(ALARM3, 0, 100);

    get_resource(RESOURCE1);
    puts("[main_task]: set_event");
    release_resource(RESOURCE1);
    set_event(SUB_TASK1, EVENT1);

    while (1) {
        if (i++ == 0x800000) {
            get_resource(RESOURCE1);
            puts("[main_task]");
            release_resource(RESOURCE1);
            i = 0;
        }
    }
}

int main()
{
    start_os();

    return 0;
}
