#include "uros.h"
#include "system.h"

extern void start();
extern void svc_handler();
extern void svc_systick();

void (* const RESET_VECTOR[])()  = {
	(void (*)())STACK_BTM,      /* initial MSP */
	start,                      /* reset vector */
	NULL,                       /* NMI vector */
	NULL,                       /* hard fault vector */
	NULL,                       /* MemManage fault vector */
	NULL,                       /* bus fault vector */
	NULL,                       /* usage fault vector */
	NULL,                       /* reserved */
	NULL,                       /* reserved */
	NULL,                       /* reserved */
	NULL,                       /* reserved */
	svc_handler,                /* SVC vector */
	NULL,                       /* debug monitor vector */
	NULL,                       /* reserved */
	NULL,                       /* PendSV vector */
	svc_systick,                /* SysTick vector */
};
