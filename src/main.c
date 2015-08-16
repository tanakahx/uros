#include "uros.h"
#include "uart.h"

int id[4];

void sub_task0(int ex)
{
    puts("[sub_task0]");
    terminate_task();
}

void sub_task1(int ex)
{
    volatile int i = 0;
    int count = 0;

    while (1) {
        if (i++ == 0x200000) {
            get_resource(0);
            puts("[sub_task1]");
            release_resource(0);
            i = 0;
            if (++count == 3) {
                get_resource(0);
                puts("[sub_task1]: set_event");
                release_resource(0);
                set_event(id[3], 0x1 << 0);
            }
        }
    }
}

void sub_task2(int ex)
{
    volatile int i = 0;
    int count = 0;

    while (1) {
        if (i++ == 0x200000) {
            get_resource(0);
            puts("[sub_task2]");
            release_resource(0);
            i = 0;
            if (++count == 3) {
                get_resource(0);
                puts("[sub_task2]: set_event");
                release_resource(0);
                set_event(id[3], 0x1 << 1);
            }
        }
    }
}

void sub_task3(int ex)
{
    uint32_t ev;

    while (1) {
        wait_event(0x3);
        get_event(id[3], &ev);
        if (ev == (0x1 << 0))
            puts("[sub_task3]: wake up by sub_task1");
        else if (ev == (0x1 << 1))
            puts("[sub_task3]: wake up by sub_task2");
        else
            puts("[sub_task3]: wake up by unknown");
        clear_event(id[3], -1);
    }
}

void main_task(int ex)
{
    puts("[main_task]: start");

    /* Create some tasks */
    id[0] = declare_task(sub_task0, 0, 64);
    id[1] = declare_task(sub_task1, 2, 256);
    id[2] = declare_task(sub_task2, 2, 256);
    id[3] = declare_task(sub_task3, 1, 256);

    /* Create resources */
    res[0].pri = 0;

    /* Start them */
    activate_task(id[0]);
    activate_task(id[0]); /* start again */
    activate_task(id[1]);
    activate_task(id[2]);
    activate_task(id[3]);

    puts("[main_task]: done");
    terminate_task();
}

int main()
{
    start_os();

    return 0;
}
