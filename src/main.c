#include "uros.h"
#include "system.h"
#include "uart.h"
#include "lib.h"

#define NR_TASK 64
#define STATE_FREE    1
#define STATE_SUSPEND 2
#define STATE_READY   4
#define STATE_RUNNING 8
#define NR_REG 10
#define PRI_MAX 255
#define TICK_TH 10
#define PEND_SV ICSR |= (1<<28)

typedef struct {
    int id;
    int state;
    int pri;
    uint32_t tick;
    uint32_t reg[NR_REG]; /* R4-R11, LR, SP */
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
    REG_LR,
    REG_SP,
};

void schedule();
int sys_switch(void *args);
int sys_debug(void *args);
int sys_task_new(void *args);
int sys_task_start(void *args);
int sys_task_exit(void *args);

int (* const syscall_table[])(void *args) = {
    sys_switch,
    sys_debug,
    sys_task_new,
    sys_task_start,
    sys_task_exit,
};

task_t task[NR_TASK];
task_t *taskp  = NULL;

void print_reg()
{
    int i;
    char *reg_name[] = {"R04", "R05", "R06", "R07",
                        "R08", "R09", "R10", "R11",
                        "LR ", "SP "};

    for (i = 0; i < NR_REG; i++) {
        printf("%s:", reg_name[i]);
        puthex_n(taskp->reg[i], 8);
        putchar('\n');
    }
}

__attribute__ ((naked))
void pendsv_handler()
{
    /* save context (R4-R11, LR, SP) */
    asm("stmia %0!, {r4-r11};" /* R4-R11 */
        "str   lr, [%0], #4;"  /* LR */
        "tst   lr, #4;"
        "ite   eq;"
        "mrseq r0, msp;"
        "mrsne r0, psp;"
        "str   r0, [%0];"      /* SP */
        "bl    schedule;"
        : 
        : "r" (taskp->reg)
        : "cc", "r0");
    
    asm("ldmia %0!, {r4-r11};"
        "ldr lr, [%0], #4;"    /* LR */
        "ldr r0, [%0], #4;"    /* SP */
        "msr msp, r0;"
        "bx lr;"
        :
        : "r" (taskp->reg) : "r0", "lr");
}

void systick_handler()
{
    if (++taskp->tick > TICK_TH) {
        taskp->state = STATE_READY;
        taskp->tick = 0;
    }
    PEND_SV;
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
        "bl svc_dispatch;");

    PEND_SV;
    
    asm("mov lr, #0xFFFFFFF9;"
        "bx lr;");
}

void schedule()
{
    task_t *p = taskp + 1;
    task_t *n = NULL;
    int pri = PRI_MAX + 1;

    /* If the current running task does not expired its tick, task switching 
       does not occur and the current running task goes on to its process. */
    if (taskp->state == STATE_RUNNING)
        return;

    /* Select the next running task from ready state tasks.
       Scheduling algorithm is priority based round robin. */
    do {
        if (p->state == STATE_READY && p->pri < pri) {
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

    eid = syscall_table[svc_number](args);
    *(int *)args = eid;
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
    uint32_t *sp;
        
    /* search a free task */
    for (p = task; p < task + NR_TASK; p++) {
        if (p->state & STATE_FREE) {
            tp = p;
            break;
        }
    }
    if (tp == NULL)
        return -1; /* ERROR: tasks are full */

    /* allocate new stack */
    sp = (uint32_t *)mem_alloc(sizeof(uint32_t)*argp[2]);
    if (sp == NULL)
        return -1; /* ERROR: stack allocation error */
    sp += argp[2] - 8;
    memset(sp, 0, sizeof(uint32_t) * 8);
    *(sp + 7) = 0x01000000;        /* xPSR */
    *(sp + 6) = argp[0]; /* Return address */
        
    /* clear all register */
    memset(tp->reg, 0, sizeof(tp->reg));

    tp->reg[REG_SP] = (uint32_t)sp;
    tp->reg[REG_LR] = 0xFFFFFFF9;
    
    tp->id    = tp - task;
    tp->state = STATE_SUSPEND;
    tp->pri   = argp[1];
    tp->tick  = 0;
    
    return tp->id;
}

int sys_task_start(void *args)
{
    int *argp = args;
    int id = argp[0];
    task[id].state = STATE_READY;
    return 0;
}

int sys_task_exit(void *args)
{
    taskp->state = STATE_FREE;
    mem_free((void *)taskp->reg[REG_SP]);
    return 0;
}

#define SYS_CALL_STUB(svc, name, ...) int name(__VA_ARGS__) {   \
        int ret;                                                \
        asm volatile ("svc %1;"                                 \
                      "mov %0, r0;"                             \
                      : "=r" (ret)                              \
                      : "I" (svc)                               \
                      : "r0", "r1", "r2", "r3");                \
        return ret;                                             \
    }

SYS_CALL_STUB(0, s_switch, void);
SYS_CALL_STUB(1, s_debug, char *dummy);
SYS_CALL_STUB(2, s_task_new, void (*dummy)(), int pri, int stack_size);
SYS_CALL_STUB(3, s_task_start, int dummy);
SYS_CALL_STUB(4, s_task_exit, int dummy);

void task_init()
{
    int i;
    for (i = 0; i < NR_TASK; i++) 
        task[i].state = STATE_FREE;

    /* set up default task */
    task[0].id = 0;
    task[0].state = STATE_RUNNING;
    task[0].pri = PRI_MAX;
    taskp = &task[0];
}

/* =================================================== */

void sub_task1()
{
    volatile int i = 0;
    while (1) {
        if (i++ == 0x2000000) {
            puts("[sub_task1]");
            i = 0;
        }
    }
}

void sub_task2()
{
    volatile int i = 0;
    while (1) {
        if (i++ == 0x2000000) {
            puts("[sub_task2]");
            i = 0;
        }
    }
}

void main_task()
{
    int id[2];
    
    /* enable systick interrupt */
    SYST_RVR = SYST_CALIB * 100;
    SYST_CSR = 0x00000007;
    
    printf("[main_task]: start\n");
    id[0] = s_task_new(sub_task1, 2, 256);
    id[1] = s_task_new(sub_task2, 2, 256);
    s_task_start(id[0]);
    s_task_start(id[1]);
    printf("[main_task]: done\n");
    s_task_exit(0);
}

int main()
{
    void *args[] = {main_task, (void *)1, (void *)256}; /* initial task's arguments */
    int id;

    task_init();
	id = sys_task_new(args); /* setup initial task */
	*(int *)args = id;
	sys_task_start(args);
	s_task_exit(0); /* does not return here */

	return 0;
}
