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
#define SVC(n) asm volatile ("svc %[i]" :: [i] "I" (n))

typedef struct {
	int id;
	int state;
	int pri;
	void (*entry)();
	uint32_t reg[NR_REG]; /* R4-R11, LR, SP */
} task_t;

int sys_switch(void *args);
int sys_debug(void *args);
int sys_task_new(void *args);
int sys_task_exit(void *args);

int (* const syscall_table[])(void *args) = {
	sys_switch,
	sys_debug,
    sys_task_new,
	sys_task_exit,
};

task_t task[NR_TASK];
int task_count = 0;
task_t *taskp  = NULL;

void print_reg()
{
	int i;
	char *reg_name[] = {"R04", "R05", "R06", "R07", "R08", "R09", "R10", "R11", "LR ", "SP "};

	for (i = 0; i < NR_REG; i++) {
		printf("%s:", reg_name[i]);
		puthex_n(taskp->reg[i], 8);
		putchar('\n');
	}
}

__attribute__ ((naked))
void svc_handler()
{
	/* save context */
	asm("stmia %0!, {r4-r11};" /* R4-R11 */
		"str   lr, [%0], #4;"  /* LR */
		"tst   lr, #4;"
		"ite   eq;"
		"mrseq r0, msp;"
		"mrsne r0, psp;"
		"str   r0, [%0];"      /* SP */
		"ldr   r1, [r0, #4*6];"
		"sub   r1, r1, #2;"
		"ldrb  r1, [r1];"
		"bl svc_dispatch;"
		: 
		: "r" (taskp->reg)
		: "cc", "r0", "r1");
	
	if (taskp->state == STATE_SUSPEND) {
		/* setup a new stack frame */
		asm("mov r0, #8;"
			"mrs r1, msp;"
			"sub r2, r1, #4*8;"
			"msr msp, r2;"
			"0: cbz r0, 1f;"
			"ldr r3, [r1], #4;"
			"str r3, [r2], #4;"
			"sub r0, #1;"
			"b 0b;"
			"1:");
		asm("mov r0, %0;"
			"str r0, [sp, #4*6];"
			"mov r0, #0xFFFFFFF9;"
			"str r0, [%1], #4;"
			"mrs r0, msp;"
			"str r0, [%1];"
			:
			: "r" (taskp->entry), "r" (&taskp->reg[8])
			: "r0");
		taskp->state = STATE_RUNNING;
	}
	
	asm("ldmia %0!, {r4-r11};"
		"ldr lr, [%0], #4;"  /* LR */
		"ldr r0, [%0], #4;"  /* SP */
		"msr msp, r0;"
		"bx lr;"
		:
		: "r" (taskp->reg) : "r0", "lr");
}

void schedule()
{
	task_t *p;
	task_t *n = NULL;
	int pri = PRI_MAX + 1;

	for (p = task; p < task + NR_TASK; p++) {
		if (p->state != STATE_FREE && p->pri < pri) {
			pri = p->pri;
			n = p;
		}
	}
	
	if (n == NULL)
		return; /* ERROR: no task is available */
	
	taskp = n;
}

void svc_dispatch(void *args, int svc_number)
{
	//printf("*** SVC call(%d) ***\n", svc_number);

	syscall_table[svc_number](args);
	// TODO: write return value of system call to a stack frame
	
	schedule();
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
 */
int sys_task_new(void *args)
{
	unsigned int *argp = args;
	task_t *p;
	task_t *n = NULL;
	
	// search a free task
	for (p = task; p < task + NR_TASK; p++) {
		if (p->state & STATE_FREE) {
			n = p;
			break;
		}
	}
	
	if (n == NULL)
		return -1; /* ERROR: tasks are full */

	n->id    = task_count++;
	n->state = STATE_SUSPEND;
	n->pri   = argp[1];
	n->entry = (void (*)())argp[0]; 
    memset(n->reg, 0, sizeof(n->reg));
	
	return n - task;
}

int sys_task_exit(void *args)
{
	taskp->state = STATE_FREE;
	return 0;
}

#define SYS_CALL_STUB(svc, name, ...) void name(__VA_ARGS__) { SVC(svc); }

SYS_CALL_STUB(0, s_switch, void);
SYS_CALL_STUB(1, s_debug, char *dummy);
SYS_CALL_STUB(2, s_task_new, void (*dummy)(), int pri);
SYS_CALL_STUB(3, s_task_exit, int dummy);

void task_init()
{
	int i;
	for (i = 0; i < NR_TASK; i++) 
		task[i].state = STATE_FREE;
}

/* =================================================== */

void another_task()
{
	printf("[another_task]: start\n");
	s_debug("hello from another task");
	printf("[another_task]: done\n");
	s_task_exit(0);
}

void main_task()
{
	printf("[main_task]: start\n");
	s_debug("hello from main task");
	printf("[main_task]: s_task_new()\n");
	s_task_new(another_task, 1);
	printf("[main_task]: done\n");
	s_task_exit(0);
}

int main()
{
	int i;
	void *args[] = {main_task, (void *)PRI_MAX};
	
	task_init();
	i = sys_task_new((unsigned int *)args); /* setup initial task */
	taskp = &task[i];
	s_switch();

	printf("system exit\n");
	for (;;) ;

	return 0;
}
