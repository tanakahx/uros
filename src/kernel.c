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

#define NR_TASK 16
#define NR_RES  8
#define PRI_MAX 255
#define TICK_TH 1

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
    uint32_t tick;
    uint32_t ev_wait;
    uint32_t ev_flag;
    wque_t wque;
    context_t context;
} task_t;

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
task_t *taskp      = NULL;
task_t *taskp_next = NULL;
res_t res[NR_RES];
uint32_t default_task_stack[DEFAULT_TASK_STACK_SIZE];

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

void systick_handler()
{
    if (taskp && ++taskp->tick > TICK_TH) {
        taskp->state = STATE_READY;
        taskp->tick = 0;
        schedule();
    }
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
        "pop   {lr, r1};"             /* Restore PSP and then on the top of the process stack frame, */
        "str   r0, [r1];"             /* write the value from system call to return it back to the calling task. */
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
    tp->tick  = 0;

    return tp->id;
}

int isys_activate_task(int task_id)
{
    task_t *tp = &task[task_id];
    uint32_t *sp = tp->stack_bottom + tp->stack_size - 16;

    /* Initialize stack frame necessary for starting in user mode */
    memset(sp, 0, sizeof(uint32_t) * 16);
    sp[15] = 0x01000000;          /* xPSR */
    sp[14] = (uint32_t)tp->entry; /* Return address */
    tp->context = (context_t)sp;

    tp->state = STATE_READY;
    tp->tick  = 0;

    return 0;
}

int sys_activate_task(int task_id)
{
    int ret = isys_activate_task(task_id);

    schedule();

    return ret;
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

void default_task(int ex)
{
    puts("[default_task]");
    while (1) ;
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

    id = sys_declare_task(main_task, 1, 256); /* Setup main task */
    isys_activate_task(id);

    taskp = taskp_next = &task[0];
}

__attribute__((naked))
void start_os(void)
{
    initialize_object();

    pend_sv();

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
