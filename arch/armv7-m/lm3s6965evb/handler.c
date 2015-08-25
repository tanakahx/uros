#include "uart.h"

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
