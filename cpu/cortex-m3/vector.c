#include "uros.h"
#include "system.h"
#include "uart.h"

static void reset_handler(void);
extern void svc_handler(void);
extern void systick_handler(void);
extern void pendsv_handler(void);
static void nmi_handler(void);
static void hard_fault_handler(void);
static void mm_handler(void);
static void bus_fault_handler(void);
static void usage_fault_handler(void);
static void debug_monitor_handler(void);

extern void initialize_system(void);

void (* const vector_table[])()  = {
    (void (*)())STACK_BTM,      /* initial MSP */
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

void reset_handler(void)
{
    /* System dependent initialization */
    initialize_system();

    /* Let's get started. */
    uros_main();
}

void nmi_handler(void)
{
    puts("[nmi_handler] Unhandled exception occured!");
    while (1) continue;
}

void hard_fault_handler(void)
{
    puts("[hard_fault_handler] Unhandled exception occured!");
    while (1) continue;
}

void mm_handler(void)
{
    puts("[mm_handler] Unhandled exception occured!");
    while (1) continue;
}

void bus_fault_handler(void)
{
    puts("[bus_fault_handler] Unhandled exception occured!");
    while (1) continue;
}

void usage_fault_handler(void)
{
    puts("[usage_fault_handler] Unhandled exception occured!");
    while (1) continue;
}

void debug_monitor_handler(void)
{
    puts("[debug_monitor_handler] Unhandled exception occured!");
    while (1) continue;
}
