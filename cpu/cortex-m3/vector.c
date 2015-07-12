#include "uros.h"
#include "system.h"

extern void start();
extern void svc_handler();

void (* const RESET_VECTOR[])()  = {
	(void (*)())0x20010000,     /* initial MSP */
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
	NULL,                       /* SysTick vector */
};
