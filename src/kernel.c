#include "uros.h"
#include "system.h"
#include "uart.h"
#include "lib.h"
#include "config.h"

#define NR_COUNTER 1

/* Wait queue */
typedef struct wque {
    struct wque *next;
    struct wque *prev;
} wque_t;

/* Resource Type */
typedef struct {
    uint32_t owner;
    int      pre_pri;
    wque_t   wque;
} res_t;

/* Task Control Block (TCB) */
typedef struct task {
    task_state_t state;
    int          pri;
    uint32_t     ev_wait;
    uint32_t     ev_flag;
    wque_t       wque;
    context_t    context;
} task_t;

/* Counter Type */
typedef struct {
    tick_t       value;
    tick_t       next_tick;
    tick_t       last_tick;
    const alarm_base_t *alarm_basep;
} counter_t;

/* Alarm Type */
typedef struct {
    alarm_state_t state;
    alarm_type_t  type;
    tick_t        next_count;
    tick_t        last_count;
    tick_t        cycle;
    bool_t        expired;
    counter_t     *counterp;
} alarm_t;

extern void main(void);

#define CHECK_ID(id, limit) if (id >= limit) return E_OS_ID

#define SYS_CALL_STUB(svc, name, ...) status_type_t name(__VA_ARGS__) { \
        int ret;                                                        \
        asm volatile ("svc %1;"                                         \
                      "mov %0, r0;"                                     \
                      : "=r" (ret)                                      \
                      : "I" (svc)                                       \
                      : "r0", "r1", "r2", "r3");                        \
        return ret;                                                     \
    }                                                                   \
    status_type_t sys_##name(__VA_ARGS__);

SYS_CALL_STUB( 0, debug, const char *s);
SYS_CALL_STUB( 1, activate_task, task_type_t task_id);
SYS_CALL_STUB( 2, terminate_task, void);
SYS_CALL_STUB( 3, chain_task, task_type_t task_id);
SYS_CALL_STUB( 4, get_task_id, task_type_t *task_id);
SYS_CALL_STUB( 5, get_task_state, task_type_t task_id, task_state_t *task_state);
SYS_CALL_STUB( 6, get_resource, uint32_t res_id);
SYS_CALL_STUB( 7, release_resource, uint32_t res_id);
SYS_CALL_STUB( 8, set_event, task_type_t task_id, event_mask_type_t event);
SYS_CALL_STUB( 9, clear_event, event_mask_type_t event);
SYS_CALL_STUB(10, get_event, task_type_t task_id, event_mask_type_t *event);
SYS_CALL_STUB(11, wait_event, event_mask_type_t event);
SYS_CALL_STUB(12, get_alarm_base, uint32_t alarm_id, alarm_base_t *alarm_base);
SYS_CALL_STUB(13, get_alarm, uint32_t alarm_id, tick_t *tick);
SYS_CALL_STUB(14, set_rel_alarm, uint32_t alarm_id, tick_t increment, tick_t cycle);
SYS_CALL_STUB(15, set_abs_alarm, uint32_t alarm_id, tick_t start, tick_t cycle);
SYS_CALL_STUB(16, cancel_alarm, uint32_t alarm_id);

static void schedule();
static void wake_up(res_t *rp);

const sys_call_t syscall_table[] = {
    (sys_call_t)sys_debug,
    (sys_call_t)sys_activate_task,
    (sys_call_t)sys_terminate_task,
    (sys_call_t)sys_chain_task,
    (sys_call_t)sys_get_task_id,
    (sys_call_t)sys_get_task_state,
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

uint32_t user_task_stack[USER_TASK_STACK_SIZE];
task_t task[NR_TASK];
res_t res[NR_RES];
counter_t counter[NR_COUNTER];
alarm_t alarm[NR_ALARM];
const alarm_base_t alarm_base[NR_COUNTER] = {
    {TICK_MAX, 10, 1,}, /* maxallowedvalue, ticksperbase, mincycle */
};

task_t *taskp      = NULL;
task_t *taskp_next = NULL;
tick_t systick;

__attribute__((naked))
void PendSV_Handler()
{
    /*
     * Save context informations.
     * r4-r11 are saved in the user stack and PSP is saved in the TCB.
     */
    asm("mrs   r0, PSP;"
        "stmdb r0!, {r4-r11};"
        "str   r0, [%0];"
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

void SysTick_Handler()
{
    alarm_t *alarmp;
    const alarm_action_rom_t *action_romp;
    counter_t *counterp;
    bool_t single_alarm;

    for (alarmp = alarm; alarmp < alarm + NR_ALARM; alarmp++) {
        if (alarmp->state == ALARM_STATE_ACTIVE) {
            /* In case of single alarms, cycle shall be zero. */
            single_alarm = !alarmp->cycle;
            if (single_alarm && alarmp->expired)
                continue;

            if (alarmp->counterp->value == alarmp->next_count) {
                alarmp->next_count += alarmp->cycle; /* This result can be overflow. */
                alarmp->last_count = alarmp->counterp->value;
                alarmp->expired = TRUE;

                action_romp = alarm_action_rom + (alarmp - alarm);

                switch (action_romp->action_type) {
                case ACTION_TYPE_ACTIVATETASK:
                    sys_activate_task(action_romp->action.task_id);
                    break;
                case ACTION_TYPE_SETEVENT:
                    sys_set_event(action_romp->action.setevent.task_id, action_romp->action.setevent.event);
                    break;
                case ACTION_TYPE_ALARMCALLBACK:
                    action_romp->action.callback();
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
void SVC_Handler()
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

    task_t *p = taskp;
    task_t *n = NULL;
    int pri = PRI_MAX + 1;

    if (taskp->state & TASK_STATE_RUNNING)
        taskp->state = TASK_STATE_READY;

    do {
        if (++p == task + NR_TASK)
            p = task;
        if ((p->state & TASK_STATE_READY) && p->pri < pri) {
            pri = p->pri;
            n = p;
        }
    } while (p != taskp);

    if (n == NULL)
        return; /* ERROR: no task is available */

    taskp_next = n;
    taskp_next->state = TASK_STATE_RUNNING;

    pend_sv();
}

status_type_t sys_debug(const char *s)
{
    printf("DEBUG MESSAGE: %s\n", s);
    return E_OK;
}

status_type_t activate(task_t *tp) 
{
    status_type_t status = E_OK;
    uint32_t *sp;
    const task_rom_t *task_romp = task_rom + (tp - task);

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (tp->state & (TASK_STATE_RUNNING | TASK_STATE_READY | TASK_STATE_WAITING))
        status = E_OS_LIMIT;
    else {
        tp->state = TASK_STATE_READY;
        tp->pri   = task_romp->pri;

        sp = task_romp->stack_bottom - 16;
        /* Initialize stack frame necessary for starting in user mode */
#ifdef DEBUG
        memset(sp, 0xBBCCDDEE, sizeof(uint32_t) * 16);
#endif
        sp[15] = 0x01000000;          /* xPSR */
        sp[14] = (uint32_t)task_romp->entry;
        tp->context = (context_t)sp;
    }

    enable_interrupt();
    /* CRITICAL SECTION: END */

    return status;
}

status_type_t sys_activate_task(task_type_t task_id)
{
    status_type_t status = E_OK;
    task_t *tp;

    CHECK_ID(task_id, NR_TASK);

    tp = &task[task_id];

    status = activate(tp);

    if (status != E_OK)
        return status;

    schedule();

    return E_OK;
}

void terminate(task_t *tp)
{
    int i;
    res_t *rp;
    task_type_t task_id = taskp - task;

    taskp->state = TASK_STATE_SUSPENDED;

    /* Clear event */
    taskp->ev_wait = 0;
    taskp->ev_flag = 0;

    /* Release all allocating resources */
    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    rp = res;
    for (i = 0; i < NR_RES; i++, rp++) {
        if (rp->owner == task_id) {
            rp->owner = 0;
            /* Wake up another task if it is waiting for this resoure */
            wake_up(rp);
        }
    }

    enable_interrupt();
    /* CRITICAL SECTION: END */
}

status_type_t sys_terminate_task(void)
{
    terminate(taskp);

    schedule();

    return E_OK;
}

status_type_t sys_chain_task(task_type_t task_id)
{
    task_t *tp;
    status_type_t status = E_OK;

    CHECK_ID(task_id, NR_TASK);

    tp = &task[task_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (tp->state != TASK_STATE_SUSPENDED)
        status = E_OS_LIMIT;
    else {
        terminate(taskp);
        activate(tp);
    }

    enable_interrupt();
    /* CRITICAL SECTION: END */

    schedule();

    return status;
}

status_type_t sys_get_task_id(task_type_t *task_id)
{
    *task_id = taskp - task;

    return E_OK;
}

status_type_t sys_get_task_state(task_type_t task_id, task_state_t *task_state)
{
    CHECK_ID(task_id, NR_TASK);

    *task_state = task[task_id].state;

    return E_OK;
}

status_type_t sys_get_resource(uint32_t res_id)
{
    status_type_t status;
    res_t *rp;
    wque_t *wp;

    CHECK_ID(res_id, NR_RES);

    rp = &res[res_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (!rp->owner) {
        /* Resource is free. Allocate it for this task. */
        status = E_OK;
        rp->owner = taskp - task;
    }
    else {
        /* Resource is already allocated. Add this task into the wait queue. */
        status = E_OS_ACCESS;

        /* Add this task into the wait queue */
        wp                    = &taskp->wque;
        wp->next              = rp->wque.next;
        wp->prev              = &rp->wque;
        rp->wque.next->prev   = wp;
        rp->wque.next         = wp;

        /* Go to wait state */
        taskp->state = TASK_STATE_WAITING;
    }

    enable_interrupt();
    /* CRITICAL SECTION: END */

    if (status == E_OK) {
        /* Raise priority to the resource priority (priority ceiling protocol) */
        rp->pre_pri = taskp->pri;
        taskp->pri  = res_rom[res_id].pri;
    }

    schedule();

    return status;
}

static bool_t task_waiting_for(res_t *rp)
{
    return rp->wque.next != &rp->wque;
}

static void wake_up(res_t *rp)
{
    wque_t *wp;
    task_t *tp;
    task_type_t task_id;
    uint32_t res_id = rp - res;

    if (task_waiting_for(rp)) {
        /* Remove the head of the wait queue to place it in the READY state. */
        wp             = rp->wque.prev;
        wp->prev->next = &rp->wque;
        rp->wque.prev  = wp->prev;
        wp->next = NULL;
        wp->prev = NULL;

        /*
         * Task id that is begin removed from the wait queue can be calculated
         * by the offset from the beggining of a task array.
         */
        task_id = ((size_t)wp - (size_t)task) / sizeof(task_t);
        tp      = &task[task_id];

        /* Allocate resource for this task */
        rp->owner = task_id;

        /* Temporarily raise priority (priority ceiling protocol) */
        rp->pre_pri = tp->pri;
        tp->pri     = res_rom[res_id].pri;

        tp->state = TASK_STATE_READY;
    }
}

status_type_t sys_release_resource(uint32_t res_id)
{
    status_type_t status = E_OK;
    res_t *rp;
    task_type_t task_id = taskp - task;

    CHECK_ID(res_id, NR_RES);

    rp = &res[res_id];

    if (rp->owner != task_id)
        return E_OS_NOFUNC;

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    /* Release resource */
    rp->owner = 0;

    /* Lower priority to the original level */
    taskp->pri = rp->pre_pri;

    /* Wake up another task if it is waiting for this resoure */
    wake_up(rp);

    enable_interrupt();
    /* CRITICAL SECTION: END */

    schedule();

    return status;
}

status_type_t sys_set_event(task_type_t task_id, event_mask_type_t event)
{
    status_type_t status = E_OK;
    task_t *tp;
    bool_t resched = FALSE;

    CHECK_ID(task_id, NR_TASK);

    tp = &task[task_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (tp->state & TASK_STATE_SUSPENDED)
        status = E_OS_STATE;
    else {
        tp->ev_flag |= event;
        if ((tp->ev_wait & tp->ev_flag) && (tp->state & TASK_STATE_WAITING)) {
            tp->ev_wait = 0;
            tp->state   = TASK_STATE_READY;
            resched = TRUE;
        }
    }

    enable_interrupt();
    /* CRITICAL SECTION: END */

    if (resched)
        schedule();

    return status;
}

status_type_t sys_clear_event(event_mask_type_t event)
{
    taskp->ev_flag &= ~event;

    return E_OK;
}

status_type_t sys_get_event(task_type_t task_id, event_mask_type_t *event)
{
    status_type_t status = E_OK;
    task_t *tp;

    CHECK_ID(task_id, NR_TASK);

    tp = &task[task_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (tp->state & TASK_STATE_SUSPENDED)
        status = E_OS_STATE;
    else
        *event = tp->ev_flag;

    enable_interrupt();
    /* CRITICAL SECTION: END */

    return status;
}

status_type_t sys_wait_event(event_mask_type_t event)
{
    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    /*
     * If the current task is interrupted by another task when it decides to branche a 'else' statement and 
     * another task executes set_wait system call at that time, racing occurs and this task will wait forever.
     */

    taskp->ev_wait = event;
    if (taskp->ev_wait & taskp->ev_flag)
        taskp->state = TASK_STATE_READY;
    else
        taskp->state = TASK_STATE_WAITING;

    enable_interrupt();
    /* CRITICAL SECTION: END */

    schedule();

    return E_OK;
}

status_type_t sys_get_alarm_base(uint32_t alarm_id, alarm_base_t *alarm_basep)
{
    const alarm_base_t *abp;

    CHECK_ID(alarm_id, NR_ALARM);

    abp = alarm[alarm_id].counterp->alarm_basep;
    alarm_basep->maxallowedvalue = abp->maxallowedvalue;
    alarm_basep->ticksperbase    = abp->ticksperbase;
    alarm_basep->mincycle        = abp->mincycle;

    return E_OK;
}

status_type_t sys_get_alarm(uint32_t alarm_id, tick_t *tickp)
{
    alarm_t *ap;
    counter_t *cp;
    const alarm_base_t *abp;
    tick_t count;
    tick_t last_count;
    tick_t tick;
    tick_t last_tick;
    tick_t tick_elapsed;
    tick_t count_elapsed;

    CHECK_ID(alarm_id, NR_ALARM);

    if (alarm[alarm_id].state == ALARM_STATE_FREE)
        return E_OS_NOFUNC;

    ap  = &alarm[alarm_id];
    cp  = ap->counterp;
    abp = cp->alarm_basep;

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    /*
     * If the values of counter and tick are incremented by SysTick interruption and cause overflow
     * during while this section, the relative value of tick cannot be calculated correctly.
     */
    count      = cp->value;
    last_count = ap->last_count;
    tick       = systick;
    last_tick  = cp->last_tick;

    enable_interrupt();
    /* CRITICAL SECTION: END */

    count_elapsed = elapsed_time(count, last_count, abp->maxallowedvalue);
    tick_elapsed  = elapsed_time(tick, last_tick, TICK_MAX);
    *tickp        = count_elapsed * abp->ticksperbase + tick_elapsed;

    return E_OK;
}

void activate_alarm(alarm_t *alarm, alarm_type_t type, tick_t next_count, tick_t cycle)
{
    alarm->type       = type;
    alarm->next_count = next_count;
    alarm->last_count = 0;
    alarm->cycle      = cycle;
    alarm->expired    = FALSE;
    alarm->state      = ALARM_STATE_ACTIVE;
}

status_type_t sys_set_rel_alarm(uint32_t alarm_id, tick_t increment, tick_t cycle)
{
    status_type_t status = E_OK;
    alarm_t *ap;
    tick_t next_count;

    CHECK_ID(alarm_id, NR_ALARM);

    ap = &alarm[alarm_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (ap->state == ALARM_STATE_ACTIVE)
        status = E_OS_STATE;
    else {
        next_count = ap->counterp->value + increment;
        activate_alarm(ap, ALARM_TYPE_REL, next_count, cycle);
    }

    enable_interrupt();
    /* CRITICAL SECTION: END */

    return status;
}

status_type_t sys_set_abs_alarm(uint32_t alarm_id, tick_t start, tick_t cycle)
{
    status_type_t status = E_OK;
    alarm_t *ap;

    CHECK_ID(alarm_id, NR_ALARM);

    ap = &alarm[alarm_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (ap->state == ALARM_STATE_ACTIVE)
        status = E_OS_STATE;
    else
        activate_alarm(ap, ALARM_TYPE_ABS, start, cycle);

    enable_interrupt();
    /* CRITICAL SECTION: END */

    return status;
}

status_type_t sys_cancel_alarm(uint32_t alarm_id)
{
    status_type_t status;
    alarm_t *ap;

    CHECK_ID(alarm_id, NR_ALARM);

    ap = &alarm[alarm_id];

    /* CRITICAL SECTION: BEGIN */
    disable_interrupt();

    if (ap->state == ALARM_STATE_FREE)
        status = E_OS_STATE;
    else
        ap->state = ALARM_STATE_FREE;

    enable_interrupt();
    /* CRITICAL SECTION: END */

    return status;
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

    /* Set up default task. */
    task[0].state       = TASK_STATE_RUNNING;
    task[0].pri         = task_rom[0].pri;
    task[0].context     = (uint32_t)(task_rom[0].stack_bottom - 16);
    ((uint32_t *)task[0].context)[15] = 0x01000000;
    ((uint32_t *)task[0].context)[14] = (uint32_t)default_task;

    /* Set up PSP to default task. */
    set_psp((uint32_t *)task[0].context + 8);

    /* Initialize user tasks */
    for (i = 1; i < NR_TASK; i++)
        if (task_rom[i].autostart)
            task[i].state = TASK_STATE_READY;
        else
            task[i].state = TASK_STATE_SUSPENDED;

    /* Wait queue */
    for (i = 0; i < NR_RES; i++) {
        res[i].wque.next = &res[i].wque;
        res[i].wque.prev = &res[i].wque;
    }

    /* Create counter */
    counter[0].alarm_basep = &alarm_base[0];
    for (i = 0; i < NR_COUNTER; i++) {
        counter[i].value = 0;
        counter[i].next_tick = counter[i].alarm_basep->ticksperbase;
        counter[i].last_tick = 0;
    }

    /* Initialize alarms */
    for (i = 0; i < NR_ALARM; i++) {
        alarm[i].state = ALARM_STATE_FREE;
        alarm[i].expired = FALSE;
        alarm[i].counterp = &counter[0];
    }

    taskp      = &task[0];
    schedule();
}

__attribute__((naked))
void start_os(void)
{
    printf("Start OS (build target: %s)\n", BUILD_TARGET_ARCH); 

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
