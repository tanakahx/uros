#include "uros.h"
#include "system.h"
#include "uart.h"
#include "lib.h"
#include "config.h"

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

extern void main(void);

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
SYS_CALL_STUB( 8, get_event, int id, uint32_t *event)
SYS_CALL_STUB( 9, wait_event, uint32_t event);

task_t task[NR_TASK];
task_t *taskp  = NULL;
res_t res[NR_RES];

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
        "ldr r0, [%0];"        /* Load SP */
        "msr psp, r0;"
        "ldr pc, =#0xFFFFFFFD;" /* unprivileged handler mode */
        :
        : "r" (taskp->reg) : "r0", "lr");
}

void systick_handler()
{
    if (taskp && ++taskp->tick > TICK_TH) {
        taskp->state = STATE_READY;
        taskp->tick = 0;
        pend_sv();
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
        "ldrb  r1, [r1];"             /* SVC number */
        "ldr   lr, [%0, r1, lsl #2];" /* Address of system call */
        "push  {r0};"
        "ldmia r0, {r0-r3};"
        "blx   lr;"
        "pop   {r1};"
        "str   r0, [r1];"
        "ldr   pc, =#0xFFFFFFFD;" /* unprivileged handler mode */
        :
        : "r"(syscall_table)
        : "r0", "r1");
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
        if (++p == task + NR_TASK)
            p = task;
    } while (p != taskp + 1);

    if (n == NULL)
        return; /* ERROR: no task is available */

    taskp = n;
    taskp->state = STATE_RUNNING;
}

int sys_debug(const char *s)
{
    print_reg();
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
    tp->state = STATE_SUSPEND;
    tp->entry = entry;
    tp->pri   = pri;
    tp->tick  = 0;

    return tp->id;
}

int sys_activate_task(int task_id)
{
    task_t *tp = &task[task_id];
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
    pend_sv();

    return 0;
}

int sys_terminate_task(void)
{
    taskp->state = STATE_FREE;

    /* necessary to reschedule */
    pend_sv();

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

    /* necessary to reschedule */
    pend_sv();

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
        /* Remove the head of the wait queue */
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

    /* necessary to reschedule */
    pend_sv();

    return 0;
}

int sys_set_event(int task_id, uint32_t event)
{
    if (!(task[task_id].state & STATE_SUSPEND)) {
        task[task_id].ev_flag |= event;
        if ((task[task_id].ev_wait & task[task_id].ev_flag) && (task[task_id].state & STATE_WAITING)) {
            task[task_id].ev_wait = 0;
            task[task_id].state = STATE_READY;

            /* necessary to reschedule */
            pend_sv();
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

    /* necessary to reschedule */
    pend_sv();

    return 0;
}

void initialize_object(void)
{
    int id;
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
     * Since default task does not require any stack frame, 
     * we don't care about reg[REG_SP].
     */
    task[0].id = 0;
    task[0].state = STATE_RUNNING;
    task[0].pri = PRI_MAX;
    taskp = &task[0];

    id = sys_declare_task(main_task, 1, 256); /* Setup main task */
    sys_activate_task(id);
}

void start_os(void)
{
    initialize_object();

    enable_interrupt();

    terminate_task();

    /* does not return here */
}

void uros_main(void)
{
    main();
}
