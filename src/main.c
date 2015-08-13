#include "uros.h"
#include "system.h"
#include "uart.h"
#include "lib.h"

#define NR_TASK 64
#define NR_RES  8
#define STATE_FREE    1
#define STATE_SUSPEND 2
#define STATE_READY   4
#define STATE_RUNNING 8
#define STATE_WAITING 16
#define NR_REG 9
#define PRI_MAX 255
#define TICK_TH 1
#define PEND_SV ICSR |= (1<<28)

/* Resource Type */
typedef struct {
    uint32_t owner;
    int pri;
} res_t;

/* Wait queue */
typedef struct wque {
    struct wque *next;
    struct wque *prev;
} wque_t;

/* Task Control Block (TCB) */
typedef struct task {
    uint32_t id;
    uint32_t state;
    void     *entry;
    uint32_t *stack_bottom;
    size_t   stack_size;
    int      pri;
    int      pre_pri;
    uint32_t tick;
    uint32_t ev_wait;
    uint32_t ev_flag;
    wque_t wque;
    uint32_t reg[NR_REG]; /* R4-R11, SP */
} task_t;

enum {
    REG_R4,
    REG_R5,
    REG_R6,
    REG_R7,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_SP,
};

int sys_switch(void *args);
int sys_debug(void *args);
int sys_task_new(void *args);
int sys_task_start(void *args);
int sys_task_exit(void *args);
int sys_get_resource(void *args);
int sys_release_resource(void *args);
int sys_set_event(void *args);
int sys_clear_event(void *args);
int sys_get_event(void *args);
int sys_wait_event(void *args);

int (* const syscall_table[])(void *args) = {
    sys_switch,
    sys_debug,
    sys_task_new,
    sys_task_start,
    sys_task_exit,
    sys_get_resource,
    sys_release_resource,
    sys_set_event,
    sys_clear_event,
    sys_get_event,
    sys_wait_event,
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

SYS_CALL_STUB( 0, s_switch, void);
SYS_CALL_STUB( 1, s_debug, char *dummy);
SYS_CALL_STUB( 2, s_task_new, void (*dummy)(), int pri, int stack_size);
SYS_CALL_STUB( 3, s_task_start, int dummy);
SYS_CALL_STUB( 4, s_task_exit, int dummy);
SYS_CALL_STUB( 5, s_get_resource, int res_id);
SYS_CALL_STUB( 6, s_release_resource, int res_id);
SYS_CALL_STUB( 7, s_set_event, int id, int ev);
SYS_CALL_STUB( 8, s_clear_event, int id, int ev);
SYS_CALL_STUB( 9, s_get_event, int id, uint32_t *ev)
SYS_CALL_STUB(10, s_wait_event, int ev);

task_t task[NR_TASK];
task_t *taskp  = NULL;
res_t res[NR_RES];
wque_t wque_base[NR_RES];
int resched = 0;

void print_reg()
{
    int i;
    char *reg_name[] = {"R04", "R05", "R06", "R07",
                        "R08", "R09", "R10", "R11",
                        "SP "};

    for (i = 0; i < NR_REG; i++) {
        printf("%s:", reg_name[i]);
        puthex_n(taskp->reg[i], 8);
        putchar('\n');
    }
}

void default_handler()
{
    puts("[default_handler] Unhandled exception occured!");
    while (1) continue;
}

__attribute__ ((naked))
void pendsv_handler()
{
    /* Save context to its TCB (R4-R11, SP) */
    asm("stmia %0!, {r4-r11};" /* Store R4-R11 */
        "mrs r0, psp;"
        "str r0, [%0];"        /* Store SP */
        "bl    schedule;"
        : 
        : "r" (taskp->reg)
        : "r0");

    /* taskp may be changed after executing schedule(). */
    
    asm("ldmia %0!, {r4-r11};"
        "mov lr, #0xFFFFFFFD;" /* unprivileged handler mode */
        "ldr r0, [%0];"        /* Load SP */
        "msr psp, r0;"
        "bx lr;"
        :
        : "r" (taskp->reg) : "r0", "lr");
}

void systick_handler()
{
    if (taskp && ++taskp->tick > TICK_TH) {
        taskp->state = STATE_READY;
        taskp->tick = 0;
        PEND_SV;
    }
}

__attribute__ ((naked))
void svc_handler()
{
    asm("tst   lr, #4;"
        "ite   eq;"
        "mrseq r0, msp;"
        "mrsne r0, psp;"
        "ldr   r1, [r0, #4*6];"
        "sub   r1, r1, #2;"
        "ldrb  r1, [r1];"
        "bl svc_dispatch;"
        "mov lr, #0xFFFFFFFD;" /* unprivileged handler mode */
        "bx lr;");
}

void schedule()
{
    task_t *p = taskp + 1;
    task_t *n = NULL;
    int pri = PRI_MAX + 1;

    if (taskp->state & STATE_RUNNING)
        taskp->state = STATE_READY;

    /*
     * this routine selects the next running task from ready state tasks.
     * Scheduling algorithm is priority based round robin.
     */
    do {
        if ((p->state & STATE_READY) && p->pri < pri) {
            pri = p->pri;
            n = p;
        }
        if (++p == task + NR_TASK - 1)
            p = task;
    } while (p != taskp + 1);

    if (n == NULL)
        return; /* ERROR: no task is available */

    taskp = n;
    taskp->state = STATE_RUNNING;
}

void svc_dispatch(void *args, int svc_number)
{
    int eid;

    printf("*** SVC call(%d) ***\n", svc_number);
    resched = 0;

    eid = syscall_table[svc_number](args);
    *(int *)args = eid;

    if (resched)
        PEND_SV;
}

int sys_switch(void *args)
{
    print_reg();
    return 0;
}

int sys_debug(void *args)
{
    char **argp  = args;
    printf("DEBUG MESSAGE: %s\n", argp[0]);
    return 0;
}

/*
 * args[0] : task entry function
 * args[1] : task priority
 * args[2] : stack size (word)
 */
int sys_task_new(void *args)
{
    unsigned int *argp = args;
    task_t *p;
    task_t *tp = NULL;
    uint32_t *stack_bottom;
    size_t stack_size = argp[2];

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
    tp->state = STATE_SUSPEND;
    tp->entry = (void *)argp[0];
    tp->pri   = argp[1];
    tp->tick  = 0;

    return tp->id;
}

int sys_task_start(void *args)
{
    int *argp = args;
    int id = argp[0];
    task_t *tp = &task[id];
    uint32_t *sp = tp->stack_bottom + tp->stack_size - 8;

    /* Clear all register */
    memset(tp->reg, 0, sizeof(tp->reg));

    /* Initialize stack values used when returning to user mode */
    memset(sp, 0, sizeof(uint32_t) * 8);
    sp[7] = 0x01000000;          /* xPSR */
    sp[6] = (uint32_t)tp->entry; /* Return address */
    tp->reg[REG_SP] = (uint32_t)sp;

    tp->state = STATE_READY;
    tp->tick  = 0;

    /* necessary to reschedule */
    resched = 1;

    return 0;
}

int sys_task_exit(void *args)
{
    taskp->state = STATE_FREE;

    /* necessary to reschedule */
    resched = 1;

    return 0;
}

int sys_get_resource(void *args)
{
    uint32_t res_id = ((uint32_t *)args)[0];
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
        wp->next                     = wque_base[res_id].next;
        wp->prev                     = &wque_base[res_id];
        wque_base[res_id].next->prev = wp;
        wque_base[res_id].next       = wp;

        /* Go to wait state */
        taskp->state = STATE_WAITING;
    }

    resched = 1;

    return 0;
}

int sys_release_resource(void *args)
{
    uint32_t res_id = ((uint32_t *)args)[0];
    uint32_t task_id;
    wque_t *wp;

    if (res[res_id].owner != taskp->id)
        return -1;

    /* Release resource */
    res[res_id].owner = 0;

    /* Lower priority to the original level */
    taskp->pri = taskp->pre_pri;

    if (wque_base[res_id].prev != &wque_base[res_id]) {
        /* Remove the head of the wait queue */
        wp = wque_base[res_id].prev;
        wp->prev->next         = &wque_base[res_id];
        wque_base[res_id].prev = wp->prev;
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

    resched = 1;

    return 0;
}

int sys_set_event(void *args)
{
    int id = ((int *)args)[0];
    uint32_t ev = ((uint32_t *)args)[1];

    if (!(task[id].state & STATE_SUSPEND)) {
        task[id].ev_flag |= ev;
        if ((task[id].ev_wait & task[id].ev_flag) && (task[id].state & STATE_WAITING)) {
            task[id].ev_wait = 0;
            task[id].state = STATE_READY;
            resched = 1;
        }
    }

    return 0;
}

int sys_clear_event(void *args)
{
    int id = ((int *)args)[0];
    uint32_t ev = ((uint32_t *)args)[1];

    task[id].ev_flag &= ~ev;

    return 0;
}

int sys_get_event(void *args)
{
    int id = ((int *)args)[0];
    uint32_t *ev = ((uint32_t **)args)[1];

    *ev = task[id].ev_flag;

    return 0;
}

int sys_wait_event(void *args)
{
    uint32_t ev = ((uint32_t *)args)[0];

    taskp->ev_wait = ev;
    if (taskp->ev_wait & taskp->ev_flag)
        taskp->state = STATE_READY;
    else
        taskp->state = STATE_WAITING;
    resched = 1;

    return 0;
}

void task_init()
{
    int i;
    for (i = 0; i < NR_TASK; i++) 
        task[i].state = STATE_FREE;

    /* 
     * Set up default task.
     * Since default task does not require any stack frame, 
     * we don't care about reg[REG_SP].
     */
    task[0].id = 0;
    task[0].state = STATE_RUNNING;
    task[0].pri = PRI_MAX;
    taskp = &task[0];

    /* Wait queue */
    for (i = 0; i < NR_RES; i++) {
        wque_base[i].next = &wque_base[i];
        wque_base[i].prev = &wque_base[i];
    }
}

/* =================================================== */

int id[4];

void sub_task0()
{
    puts("[sub_task0]");
    s_task_exit(0);
}

void sub_task1()
{
    volatile int i = 0;
    int count = 0;

    while (1) {
        if (i++ == 0x200000) {
            s_get_resource(0);
            puts("[sub_task1]");
            s_release_resource(0);
            i = 0;
            if (++count == 3) {
                puts("[sub_task1]: set_event");
                s_set_event(id[3], 0x1 << 0);
            }
        }
    }
}

void sub_task2()
{
    volatile int i = 0;
    int count = 0;

    while (1) {
        if (i++ == 0x200000) {
            s_get_resource(0);
            puts("[sub_task2]");
            s_release_resource(0);
            i = 0;
            if (++count == 3) {
                puts("[sub_task2]: set_event");
                s_set_event(id[3], 0x1 << 1);
            }
        }
    }
}

void sub_task3()
{
    uint32_t ev;

    while (1) {
        s_wait_event(0x3);
        s_get_event(id[3], &ev);
        if (ev == (0x1 << 0))
            puts("[sub_task3]: wake up by sub_task1");
        else if (ev == (0x1 << 1))
            puts("[sub_task3]: wake up by sub_task2");
        else
            puts("[sub_task3]: wake up by unknown");
        s_clear_event(id[3], -1);
    }
}

void main_task()
{
    /* Enable systick interrupt */
    SYST_RVR = SYST_CALIB * 1;
    SYST_CSR = 0x00000007;
    
    puts("[main_task]: start");

    /* Create some tasks */
    id[0] = s_task_new(sub_task0, 0, 64);
    id[1] = s_task_new(sub_task1, 2, 256);
    id[2] = s_task_new(sub_task2, 2, 256);
    id[3] = s_task_new(sub_task3, 1, 256);

    /* Create resources */
    res[0].pri = 0;

    /* Start them */
    s_task_start(id[0]);
    s_task_start(id[0]); /* start again */
    s_task_start(id[1]);
    s_task_start(id[2]);
    s_task_start(id[3]);

    puts("[main_task]: done");
    s_task_exit(0);
}

int main()
{
    void *args[] = {main_task, (void *)1, (void *)256}; /* initial task's arguments */
    int id;

    task_init();
    id = sys_task_new(args); /* Setup main task */
    *(int *)args = id;
    sys_task_start(args);
    s_task_exit(0); /* does not return here */

    return 0;
}
