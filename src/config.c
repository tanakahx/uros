#include "config.h"

const task_rom_t task_rom[NR_TASK] = {
    {default_task, PRI_MAX, default_task_stack + DEFAULT_TASK_STACK_SIZE, TRUE},
    {main_task,          1, user_task_stack + USER_TASK_STACK_SIZE - 256*0, FALSE},
    {sub_task0,          0, user_task_stack + USER_TASK_STACK_SIZE - 256*1, FALSE},
    {sub_task1,          2, user_task_stack + USER_TASK_STACK_SIZE - 256*2, FALSE},
    {sub_task2,          2, user_task_stack + USER_TASK_STACK_SIZE - 256*3, FALSE},
    {sub_task3,          1, user_task_stack + USER_TASK_STACK_SIZE - 256*4, FALSE},
};

const res_rom_t res_rom[NR_RES] = {
    {0},
};

const alarm_action_rom_t alarm_action_rom[NR_ALARM] = {
    {ACTION_TYPE_ACTIVATETASK, {{MAIN_TASK, 0}}},
    {ACTION_TYPE_SETEVENT, {{MAIN_TASK, 1}}},
    {ACTION_TYPE_ALARMCALLBACK, {{(int)main_task_callback, 0}}},
};
