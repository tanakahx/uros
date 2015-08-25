#ifndef UROS_H
#define UROS_H

#include "stdtype.h"

typedef unsigned int task_type_t;
typedef unsigned int context_t;

typedef unsigned int event_mask_type_t;

typedef unsigned long tick_t;
#define TICK_MAX ((tick_t)-1)

typedef enum {
    E_OK          = 0,
    E_OS_ACCESS   = 1,
    E_OS_CALLEVEL = 2,
    E_OS_ID       = 3,
    E_OS_LIMIT    = 4,
    E_OS_NOFUNC   = 5,
    E_OS_RESOURCE = 6,
    E_OS_STATE    = 7,
    E_OS_VALUE    = 8,
} status_type_t;

typedef status_type_t (*sys_call_t)(void);
typedef void (*thread_t)(int);
typedef void (*callback_t)(void);

typedef enum {
    TASK_STATE_FREE      = 1,
    TASK_STATE_SUSPENDED = 2,
    TASK_STATE_READY     = 4,
    TASK_STATE_RUNNING   = 8,
    TASK_STATE_WAITING   = 16,
} task_state_t;

typedef enum {
    ACTION_TYPE_ACTIVATETASK,
    ACTION_TYPE_SETEVENT,
    ACTION_TYPE_ALARMCALLBACK,
} action_type_t;

typedef enum {
    ALARM_TYPE_REL,
    ALARM_TYPE_ABS,
} alarm_type_t;

typedef enum {
    ALARM_STATE_FREE,
    ALARM_STATE_ACTIVE,
} alarm_state_t;

typedef struct {
    tick_t maxallowedvalue;
    tick_t ticksperbase;
    tick_t mincycle;
} alarm_base_t;

status_type_t debug(const char *s);
status_type_t declare_task(thread_t entry, int pri, size_t stack_size);
status_type_t activate_task(task_type_t task_id);
status_type_t terminate_task(void);
status_type_t chain_task(task_type_t task_id);
status_type_t get_task_id(task_type_t *task_id);
status_type_t get_task_state(task_type_t task_id, task_state_t *task_state);
status_type_t get_resource(uint32_t res_id);
status_type_t release_resource(uint32_t res_id);
status_type_t set_event(task_type_t task_id, event_mask_type_t event);
status_type_t clear_event(event_mask_type_t event);
status_type_t get_event(task_type_t task_id, event_mask_type_t *event);
status_type_t wait_event(event_mask_type_t event);
status_type_t get_alarm_base(uint32_t alarm_id, alarm_base_t *alarm_base);
status_type_t get_alarm(uint32_t alarm_id, tick_t *tick);
status_type_t set_rel_alarm(uint32_t alarm_id, tick_t increment, tick_t cycle);
status_type_t set_abs_alarm(uint32_t alarm_id, tick_t start, tick_t cycle);
status_type_t cancel_alarm(uint32_t alarm_id);

void start_os(void);

void uros_main(void);

#endif
