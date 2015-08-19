#include "uros.h"
#include "uart.h"

task_type_t id[5];

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
                set_event(id[4], 0x1 << 0);
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
                set_event(id[4], 0x1 << 1);
            }
        }
    }
}

void sub_task3(int ex)
{
    uint32_t ev;

    while (1) {
        wait_event(0x3);
        get_event(id[4], &ev);
        if (ev == (0x1 << 0))
            puts("[sub_task3]: wake up by sub_task1");
        else if (ev == (0x1 << 1))
            puts("[sub_task3]: wake up by sub_task2");
        else
            puts("[sub_task3]: wake up by unknown");
        clear_event(-1);
    }
}

int called = 0;

void main_task_callback(void)
{
    called = 1;
}

void main_task(int ex)
{
    tick_t t;
    puts("[main_task]: start");
    get_alarm(0, &t);
    printf("[main_task] alarm 0 is %d tick\n", t);

    puts("[main_task]: waiting alarm event");
    wait_event(0x1);
    get_alarm(0, &t);
    printf("[main_task] alarm 0 is %d tick\n", t);

    puts("[main_task]: waiting alarm callback");
    while (!called) ;
    get_alarm(0, &t);
    printf("[main_task] alarm 0 is %d tick\n", t);

    cancel_alarm(0);
    cancel_alarm(1);
    cancel_alarm(2);

    /* Start them */
    activate_task(id[1]);
    activate_task(id[1]); /* start again */
    activate_task(id[2]);
    activate_task(id[3]);
    activate_task(id[4]);

    puts("[main_task]: done");
    terminate_task();
}

int main()
{
    start_os();

    return 0;
}
