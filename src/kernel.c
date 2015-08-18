#include "uros.h"
#include "system.h"
#include "uart.h"
#include "lib.h"
#include "config.h"

#define STATE_FREE      1
#define STATE_SUSPENDED 2
#define STATE_READY     4
#define STATE_RUNNING   8
#define STATE_WAITING   16

#define NR_TASK  16
#define NR_RES   8
#define NR_COUNTER 1
#define NR_ALARM 3
#define PRI_MAX  255

#define DEFAULT_TASK_STACK_SIZE 64

/* Task Control Block (TCB) */
typedef struct task {
    uint32_t id;
    uint32_t state;
    void     *entry;
    uint32_t *stack_bottom;
    size_t   stack_size;
    int      pri;
    int      pre_pri;
    uint32_t ev_wait;
    uint32_t ev_flag;
    wque_t wque;
    context_t context;
} task_t;

extern void main(void);
extern int id[];

int sys_debug(const char *s);
int sys_declare_task(thread_t entry, int pri, size_t stack_size);
int sys_activate_task(int task_id);
int sys_terminate_task(void);
int sys_get_resource(uint32_t res_id);
int sys_release_resource(uint32_t res_id);
int sys_set_event(int task_id, uint32_t event);
int sys_clear_event(int task_id, uint32_t event);
int sys_get_event(int task_id, uint32_t *event);
int sys_wait_event(uint32_t event);
int sys_get_alarm_base(uint32_t alarm_id, alarm_base_t *alarm_base);
int sys_get_alarm(uint32_t alarm_id, tick_t *tick);
int sys_set_rel_alarm(uint32_t alarm_id, tick_t increment, tick_t cycle);
int sys_set_abs_alarm(uint32_t alarm_id, tick_t start, tick_t cycle);
int sys_cancel_alarm(uint32_t alarm_id);

static void schedule();

const sys_call_t syscall_table[] = {
    (sys_call_t)sys_debug,
    (sys_call_t)sys_declare_task,
    (sys_call_t)sys_activate_task,
    (sys_call_t)sys_terminate_task,
    (sys_call_t)sys_get_resource,
    (sys_call_t)sys_release_resource,
    (sys_call_t)sys_set_event,
    (sys_call_t)sys_clear_event,
    (sys_call_t)sys_get_event,
    (sys_call_t)sys_wait_event,
    (sys_call_t)sys_get_alarm_base,
    (sys_call_t)sys_get_alarm,
    (sys_call_t)sys_set_rel_alarm,
    (sys_call_t)sys_set_abs_alarm,
    (sys_call_t)sys_cancel_alarm,
};

#define SYS_CALL_STUB(svc, name, ...) int name(__VA_ARGS__) {   \
        int ret;                                                \
        asm volatile ("svc %1;"                                 \
                      "mov %0, r0;"                             \
                      : "=r" (ret)                              \
                      : "I" (svc)                               \
                      : "r0", "r1", "r2", "r3");                \
        return ret;                                             \
    }

SYS_CALL_STUB( 0, debug, const char *s);
SYS_CALL_STUB( 1, declare_task, thread_t entry, int pri, size_t stack_size);
SYS_CALL_STUB( 2, activate_task, int task_id);
SYS_CALL_STUB( 3, terminate_task);
SYS_CALL_STUB( 4, get_resource, uint32_t res_id);
SYS_CALL_STUB( 5, release_resource, uint32_t res_id);
SYS_CALL_STUB( 6, set_event, int id, uint32_t event);
SYS_CALL_STUB( 7, clear_event, int id, uint32_t event);
SYS_CALL_STUB( 8, get_event, int id, uint32_t *event);
SYS_CALL_STUB( 9, wait_event, uint32_t event);
SYS_CALL_STUB(10, get_alarm_base, uint32_t alarm_id, alarm_base_t *alarm_base);
SYS_CALL_STUB(11, get_alarm, uint32_t alarm_id, tick_t *tick);
SYS_CALL_STUB(12, set_rel_alarm, uint32_t alarm_id, tick_t increment, tick_t cycle);
SYS_CALL_STUB(13, set_abs_alarm, uint32_t alarm_id, tick_t start, tick_t cycle);
SYS_CALL_STUB(14, cancel_alarm, uint32_t alarm_id);

task_t task[NR_TASK];
task_t *taskp      = NULL;
task_t *taskp_next = NULL;
res_t res[NR_RES];
uint32_t default_task_stack[DEFAULT_TASK_STACK_SIZE];
tick_t systick;
counter_t counter[NR_COUNTER];
alarm_base_t alarm_base[NR_COUNTER] = {
    {TICK_MAX, 10, 1,}, /* maxallowedvalue, ticksperbase, mincycle */
};
alarm_t alarm[NR_ALARM];

__attribute__((naked))
void pendsv_handler()
{
    /*
     * Save context informations.
     * r4-r11 are saved in the user stack and PSP is saved in the TCB.
     * If PSP is null, it means that the PendSV is executed from the kernel to dispatch default task.
     * Therefore we sould skip context saving.
     */
    asm("mrs     r0, PSP;"
        "cmp     r0, #0;"
        "itt     ne;"                 /* Iff PSP is not null, we save the context. */
        "stmdbne r0!, {r4-r11};"
        "strne   r0, [%0];"
        : 
        : "r" (&taskp->context)
        : "r0");

    taskp = taskp_next;

    asm("ldmia %0!, {r4-r11};"
        "msr   PSP, %0;"
        "orr   lr, #0xD;"             /* Return back to user mode (0xFFFFFFFD) */
        "bx    lr;"
        :
        : "r" (taskp->context));
}

static tick_t elapsed_time(tick_t now, tick_t last, tick_t max_value)
{
    tick_t elapse;

    if (last <= now)
        elapse = now - last;
    else
        elapse = now + (max_value - last); /* wrap arounded */

    return elapse;
}

void systick_handler()
{
    alarm_t *alarmp;
    counter_t *counterp;
    bool_t single_alarm;

    for (alarmp = alarm; alarmp < alarm + NR_ALARM; alarmp++) {
        if (alarmp->state == ALARM_STATE_ACTIVE) {
            /* In case of single alarms, cycle shall be zero. */
            single_alarm = !alarmp->cycle;
            if (single_alarm && alarmp->expired)
                continue;

            if (alarmp->counterp->value == alarmp->next_count) {
                if (alarmp->type == ALARM_TYPE_REL)
                    alarmp->next_count += alarmp->cycle; /* This result can be overflow. */

                alarmp->last_count = alarmp->counterp->value;
                alarmp->expired = TRUE;

                switch (alarmp->action_type) {
                case ACTION_TYPE_ACTIVATETASK:
                    sys_activate_task(alarmp->action.task_id);
                    break;
                case ACTION_TYPE_SETEVENT:
                    sys_set_event(alarmp->action.setevent.task_id, alarmp->action.setevent.event);
                    break;
                case ACTION_TYPE_ALARMCALLBACK:
                    alarmp->action.callback();
                    break;
                }
            }
        }
    }

    for (counterp = counter; counterp < counter + NR_COUNTER; counterp++) {
        if (systick == counterp->next_tick) {
            if (counterp->value++ == counterp->alarm_basep->maxallowedvalue)
                counterp->value = 0;
            counterp->next_tick += counterp->alarm_basep->ticksperbase;
            counterp->last_tick = systick;
        }
    }

    /* Systick is free running. */
    systick++;
}

__attribute__((naked))
void svc_handler()
{
    asm("push  {lr};"
        "mrs   r1, PSP;"
        "ldr   r0, [r1, #4*6];"
        "sub   r0, r0, #2;"
        "ldrb  r0, [r0];"             /* SVC number */
        "ldr   lr, [%0, r0, lsl #2];" /* Address of system call */
        "push  {r1};"                 /* Save PSP on the top of main stack temporarily */
        "ldmia r1, {r0-r3};"          /* Set up arguments to be passed to system call */
        "blx   lr;"                   /* Call system call */
        "pop   {r1};"                 /* Restore PSP and then on the top of the process stack frame, */
        "str   r0, [r1];"             /* write the value from system call to return it back to the calling task. */
        "pop   {lr};"
        "orr   lr, #0xD;"             /* Return back to user mode (0xFFFFFFFD) */
        "bx    lr;"
        :
        : "r"(syscall_table)
        : "r0", "r1");
}

void schedule()
{
    /*
     * This routine selects the next running task from ready state tasks.
     * Scheduling algorithm is priority based round robin.
     * If SysTick timer is invoked while it is running, this routine may be reentered.
     * This situation has no problem for now, because current implementation has no ready queue
     * and the operation on the data structure like a linked list do not exist.
     */

    task_t *p = taskp + 1;
    task_t *n = NULL;
    int pri = PRI_MAX + 1;

    if (taskp->state & STATE_RUNNING)
        taskp->state = STATE_READY;

    do {
        if ((p->state & STATE_READY) && p->pri < pri) {
            pri = p->pri;
            n = p;
        }
        if (++p == task + NR_TASK)
            p = task;
    } while (p != taskp + 1);

    if (n == NULL)
        return; /* ERROR: no task is available */

    taskp_next = n;
    taskp_next->state = STATE_RUNNING;

    pend_sv();
}

int sys_debug(const char *s)
{
    printf("DEBUG MESSAGE: %s\n", s);
    return 0;
}

int sys_declare_task(thread_t entry, int pri, size_t stack_size)
{
    task_t *p;
    task_t *tp = NULL;
    uint32_t *stack_bottom;

    /* Search a free task */
    for (p = task; p < task + NR_TASK; p++) {
        if (p->state & STATE_FREE) {
            tp = p;
            break;
        }
    }
    if (tp == NULL)
        return -1; /* ERROR: tasks are full */

    /* Allocate new stack */
    stack_bottom = (uint32_t *)mem_alloc(sizeof(uint32_t) * stack_size);
    if (stack_bottom == NULL)
        return -1; /* ERROR: stack allocation error */
    tp->stack_bottom = stack_bottom;
    tp->stack_size   = stack_size;

    tp->id    = tp - task;
    tp->state = STATE_SUSPENDED;
    tp->entry = entry;
    tp->pri   = pri;

    return tp->id;
}

int sys_activate_task(int task_id)
{
    task_t *tp = &task[task_id];
    uint32_t *sp = tp->stack_bottom + tp->stack_size - 16;

    /* Initialize stack frame necessary for starting in user mode */
    memset(sp, 0, sizeof(uint32_t) * 16);
    sp[15] = 0x01000000;          /* xPSR */
    sp[14] = (uint32_t)tp->entry; /* Return address */
    tp->context = (context_t)sp;
    tp->state = STATE_READY;

    schedule();

    return 0;
}

int sys_terminate_task(void)
{
    taskp->state = STATE_FREE;

    schedule();

    return 0;
}

int sys_get_resource(uint32_t res_id)
{
    uint32_t task_id = taskp->id;
    wque_t *wp;

    if (!res[res_id].owner) {
        /* Allocate resource for this task */
        res[res_id].owner = task_id;

        /* Raise priority to the resource priority (priority ceiling protocol) */
        taskp->pre_pri = taskp->pri;
        taskp->pri = res[res_id].pri;
    }
    else {
        /* Add this task into the wait queue */
        wp = &task[task_id].wque;
        wp->next                    = res[res_id].wque.next;
        wp->prev                    = &res[res_id].wque;
        res[res_id].wque.next->prev = wp;
        res[res_id].wque.next       = wp;

        /* Go to wait state */
        taskp->state = STATE_WAITING;
    }

    schedule();

    return 0;
}

int sys_release_resource(uint32_t res_id)
{
    uint32_t task_id;
    wque_t *wp;

    if (res[res_id].owner != taskp->id)
        return -1;

    /* Release resource */
    res[res_id].owner = 0;

    /* Lower priority to the original level */
    taskp->pri = taskp->pre_pri;

    if (res[res_id].wque.prev != &res[res_id].wque) {
        /* Remove the head of the wait queue to place it in the READY state. */
        wp = res[res_id].wque.prev;
        wp->prev->next        = &res[res_id].wque;
        res[res_id].wque.prev = wp->prev;
        wp->next = NULL;
        wp->prev = NULL;

        /*
         * Task id that is begin removed from the wait queue can be calculated
         * by the offset from the beggining of a task array.
         */
        task_id = ((size_t)wp - (size_t)task) / sizeof(task_t);

        /* Allocate resource for this task */
        res[res_id].owner = task_id;

        /* Temporarily raise priority (priority ceiling protocol) */
        task[task_id].pre_pri = task[task_id].pri;
        task[task_id].pri = res[res_id].pri;

        task[task_id].state = STATE_READY;
    }

    schedule();

    return 0;
}

int sys_set_event(int task_id, uint32_t event)
{
    if (!(task[task_id].state & STATE_SUSPENDED)) {
        task[task_id].ev_flag |= event;
        if ((task[task_id].ev_wait & task[task_id].ev_flag) && (task[task_id].state & STATE_WAITING)) {
            task[task_id].ev_wait = 0;
            task[task_id].state = STATE_READY;

            schedule();
        }
    }

    return 0;
}

int sys_clear_event(int task_id, uint32_t event)
{
    task[task_id].ev_flag &= ~event;

    return 0;
}

int sys_get_event(int task_id, uint32_t *event)
{
    *event = task[task_id].ev_flag;

    return 0;
}

int sys_wait_event(uint32_t event)
{
    taskp->ev_wait = event;
    if (taskp->ev_wait & taskp->ev_flag)
        taskp->state = STATE_READY;
    else
        taskp->state = STATE_WAITING;

    schedule();

    return 0;
}

int sys_get_alarm_base(uint32_t alarm_id, alarm_base_t *alarm_basep)
{
    alarm_base_t *p;

    if (alarm_id >= NR_ALARM)
        return -1;
    
    p = alarm[alarm_id].counterp->alarm_basep;
    alarm_basep->maxallowedvalue = p->maxallowedvalue;
    alarm_basep->ticksperbase    = p->ticksperbase;
    alarm_basep->mincycle        = p->mincycle;

    return 0;
}

int sys_get_alarm(uint32_t alarm_id, tick_t *tickp)
{
    alarm_t *alarmp;
    counter_t *counterp;
    alarm_base_t *alarm_basep;
    tick_t count;
    tick_t last_count;
    tick_t tick;
    tick_t last_tick;
    tick_t tick_elapsed;
    tick_t count_elapsed;

    if (alarm_id >= NR_ALARM)
        return -1;

    if (alarm[alarm_id].state == ALARM_STATE_FREE)
        return -2;
    
    alarmp      = &alarm[alarm_id];
    counterp    = alarmp->counterp;
    alarm_basep = counterp->alarm_basep;

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    /*
     * If the values of counter and tick are incremented by SysTick interruption and cause overflow
     * during while this section, the relative value of tick cannot be calculated correctly.
     */
    count      = counterp->value;
    last_count = alarmp->last_count;
    tick       = systick;
    last_tick  = counterp->last_tick;

    enable_interrupt();
    /* CRITICAL SECTION: END */

    count_elapsed = elapsed_time(count, last_count, alarm_basep->maxallowedvalue);
    tick_elapsed  = elapsed_time(tick, last_tick, TICK_MAX);
    *tickp        = count_elapsed * alarm_basep->ticksperbase + tick_elapsed;

    return 0;
}

void activate_alarm(alarm_t *alarm, alarm_type_t type, tick_t next_count, tick_t cycle)
{
    alarm->state      = ALARM_STATE_ACTIVE;
    alarm->type       = type;
    alarm->next_count = next_count;
    alarm->last_count = 0;
    alarm->cycle      = cycle;
    alarm->expired    = FALSE;
}

int sys_set_rel_alarm(uint32_t alarm_id, tick_t increment, tick_t cycle)
{
    tick_t next_count;

    if (alarm_id >= NR_ALARM)
        return -1;

    if (alarm[alarm_id].state == ALARM_STATE_ACTIVE)
        return -2;

    next_count = alarm[alarm_id].counterp->value + increment;
    activate_alarm(&alarm[alarm_id], ALARM_TYPE_REL, next_count, cycle);

    return 0;
}

int sys_set_abs_alarm(uint32_t alarm_id, tick_t start, tick_t cycle)
{
    if (alarm_id >= NR_ALARM)
        return -1;

    if (alarm[alarm_id].state == ALARM_STATE_ACTIVE)
        return -2;

    activate_alarm(&alarm[alarm_id], ALARM_TYPE_ABS, start, cycle);

    return 0;
}

int sys_cancel_alarm(uint32_t alarm_id)
{
    if (alarm_id >= NR_ALARM)
        return -1;

    if (alarm[alarm_id].state == ALARM_STATE_FREE)
        return -2;

    alarm[alarm_id].state = ALARM_STATE_FREE;
    
    return 0;
}

void default_task(int ex)
{
    puts("[default_task]");
    set_rel_alarm(0, 30, 0);
    set_rel_alarm(1, 60, 0);
    set_rel_alarm(2, 90, 0);
    while (1) ;
}

void initialize_object(void)
{
    int i;

    for (i = 0; i < NR_TASK; i++) 
        task[i].state = STATE_FREE;

    /* Wait queue */
    for (i = 0; i < NR_RES; i++) {
        res[i].wque.next = &res[i].wque;
        res[i].wque.prev = &res[i].wque;
    }

    /* 
     * Set up default task.
     */
    task[0].id           = 0;
    task[0].state        = STATE_RUNNING;
    task[0].pri          = PRI_MAX;
    task[0].entry        = default_task;
    task[0].stack_bottom = default_task_stack;
    task[0].stack_size   = DEFAULT_TASK_STACK_SIZE;
    task[0].context      = (uint32_t)&default_task_stack[DEFAULT_TASK_STACK_SIZE-16];
    default_task_stack[DEFAULT_TASK_STACK_SIZE-1] = 0x01000000;
    default_task_stack[DEFAULT_TASK_STACK_SIZE-2] = (uint32_t)default_task;

    /* Create some tasks */
    id[0] = sys_declare_task(main_task, 1, 256); /* Setup main task */
    id[1] = sys_declare_task(sub_task0, 0, 256);
    id[2] = sys_declare_task(sub_task1, 2, 256);
    id[3] = sys_declare_task(sub_task2, 2, 256);
    id[4] = sys_declare_task(sub_task3, 1, 256);

    /* Create counter */
    counter[0].alarm_basep = &alarm_base[0];
    for (i = 0; i < NR_COUNTER; i++) {
        counter[i].value = 0;
        counter[i].next_tick = counter[i].alarm_basep->ticksperbase;
        counter[i].last_tick = 0;
    }

    /* Create alarm */
    alarm[0].state           = ALARM_STATE_FREE;
    alarm[0].expired         = FALSE;
    alarm[0].action_type     = ACTION_TYPE_ACTIVATETASK;
    alarm[0].action.task_id  = id[0];
    alarm[0].counterp        = &counter[0];

    alarm[1].state           = ALARM_STATE_FREE;
    alarm[1].expired         = FALSE;
    alarm[1].action_type     = ACTION_TYPE_SETEVENT;
    alarm[1].action.task_id  = id[0];
    alarm[1].action.setevent.task_id = id[0];
    alarm[1].action.setevent.event   = 0x1 << 0;
    alarm[1].counterp        = &counter[0];

    alarm[2].state           = ALARM_STATE_FREE;
    alarm[2].expired         = FALSE;
    alarm[2].action_type     = ACTION_TYPE_ALARMCALLBACK;
    alarm[2].action.task_id  = id[0];
    alarm[2].action.callback = main_task_callback;
    alarm[2].counterp        = &counter[0];

    taskp = &task[0];

    sys_activate_task(id[0]); 

    taskp_next = &task[0];
}

__attribute__((naked))
void start_os(void)
{
    initialize_object();

    /* Enable systick interrupt */
    SYST_RVR = SYST_CALIB * 1;
    SYST_CSR = 0x00000007;

    enable_interrupt();

    /* does not return here */
}

void uros_main(void)
{
    main();
}
