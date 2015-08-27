#ifndef CONFIG_H
#define CONFIG_H

#include "uros.h"

#define NR_TASK   6
#define NR_RES    1
#define NR_ALARM  3

#define MAIN_TASK 1
#define SUB_TASK0 2
#define SUB_TASK1 3
#define SUB_TASK2 4
#define SUB_TASK3 5

void default_task(int ex);
void main_task(int ex);
void sub_task0(int ex);
void sub_task1(int ex);
void sub_task2(int ex);
void sub_task3(int ex);
void main_task_callback(void);

extern const task_rom_t task_rom[];
extern const res_rom_t res_rom[];
extern const alarm_action_rom_t alarm_action_rom[];
extern uint32_t default_task_stack[];
extern uint32_t user_task_stack[];

#endif
