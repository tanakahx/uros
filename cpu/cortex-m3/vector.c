#include "uros.h"
#include "system.h"

extern void start();
extern void svc_handler();
extern void systick_handler();
extern void pendsv_handler();
extern void default_handler();

void (* const RESET_VECTOR[])()  = {
    (void (*)())STACK_BTM,      /* initial MSP */
    start,                      /* reset vector */
    default_handler,            /* NMI vector */
    default_handler,            /* hard fault vector */
    default_handler,            /* MemManage fault vector */
    default_handler,            /* bus fault vector */
    default_handler,            /* usage fault vector */
    NULL,                       /* reserved */
    NULL,                       /* reserved */
    NULL,                       /* reserved */
    NULL,                       /* reserved */
    svc_handler,                /* SVC vector */
    default_handler,            /* debug monitor vector */
    NULL,                       /* reserved */
    pendsv_handler,             /* PendSV vector */
    systick_handler,            /* SysTick vector */
};
