#include "stdtype.h"

extern unsigned int stack_bottom;

void reset_handler(void) __attribute__((weak));
void nmi_handler(void) __attribute__((weak));
void hard_fault_handler(void) __attribute__((weak));
void mm_handler(void) __attribute__((weak));
void bus_fault_handler(void) __attribute__((weak));
void usage_fault_handler(void) __attribute__((weak));
void svc_handler(void) __attribute__((weak));
void debug_monitor_handler(void) __attribute__((weak));
void pendsv_handler(void) __attribute__((weak));
void systick_handler(void) __attribute__((weak));

void (* const vector_table[])()  = {
    (void (*)())&stack_bottom,  /* initial MSP */
    reset_handler,              /* reset vector */
    nmi_handler,                /* NMI vector */
    hard_fault_handler,         /* hard fault vector */
    mm_handler,                 /* MemManage fault vector */
    bus_fault_handler,          /* bus fault vector */
    usage_fault_handler,        /* usage fault vector */
    NULL,                       /* reserved */
    NULL,                       /* reserved */
    NULL,                       /* reserved */
    NULL,                       /* reserved */
    svc_handler,                /* SVC vector */
    debug_monitor_handler,      /* debug monitor vector */
    NULL,                       /* reserved */
    pendsv_handler,             /* PendSV vector */
    systick_handler,            /* SysTick vector */
};
