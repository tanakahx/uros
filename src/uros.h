#ifndef UROS_H
#define UROS_H

#define NULL (void *)0
#define HEAP_SIZE      4096

typedef unsigned long  size_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

typedef unsigned int context_t;
typedef int (*sys_call_t)(void);
typedef void (*thread_t)(int);
typedef void (*callback_t)(void);

typedef unsigned long tick_t;
#define TICK_MAX ((tick_t)-1)

typedef unsigned char bool_t;
enum {
    FALSE,
    TRUE,
};

/* Wait queue */
typedef struct wque {
    struct wque *next;
    struct wque *prev;
} wque_t;

/* Resource Type */
typedef struct {
    uint32_t owner;
    int      pri;
    wque_t   wque;
} res_t;

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

/* Counter Type */
typedef struct {
    tick_t       value;
    tick_t       next_tick;
    tick_t       last_tick;
    alarm_base_t *alarm_basep;
} counter_t;

/* Alarm Type */
typedef struct {
    alarm_state_t state;
    alarm_type_t  type;
    tick_t        next_count;
    tick_t        last_count;
    tick_t        cycle;
    bool_t        expired;
    action_type_t action_type;
    union {
        int task_id;
        struct {
            int      task_id;
            uint32_t event;
        } setevent;
        callback_t callback;
    } action;
    counter_t     *counterp;
} alarm_t;

extern res_t res[];

int debug(const char *s);
int declare_task(thread_t entry, int pri, size_t stack_size);
int activate_task(int task_id);
int terminate_task(void);
int get_resource(uint32_t res_id);
int release_resource(uint32_t res_id);
int set_event(int task_id, uint32_t event);
int clear_event(int task_id, uint32_t event);
int get_event(int task_id, uint32_t *event);
int wait_event(uint32_t event);
int get_alarm_base(uint32_t alarm_id, alarm_base_t *alarm_base);
int get_alarm(uint32_t alarm_id, tick_t *tick);
int set_rel_alarm(uint32_t alarm_id, tick_t increment, tick_t cycle);
int set_abs_alarm(uint32_t alarm_id, tick_t start, tick_t cycle);
int cancel_alarm(uint32_t alarm_id);
void start_os(void);

void uros_main(void);

#endif
