#include "uros.h"
#include "system.h"
#include "uart.h"

static void reset_handler(void);
static void default_handler(void);
extern void svc_handler();
extern void systick_handler();
extern void pendsv_handler();
extern void default_handler();

void (* const vector_table[])()  = {
    (void (*)())STACK_BTM,      /* initial MSP */
    reset_handler,              /* reset vector */
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

void reset_handler(void)
{
    start_os();
}

void default_handler(void)
{
    /* temporal use of UART for debug */
    puts("[default_handler] Unhandled exception occured!");
    while (1) continue;
}
