#include "uart.h"

void Nmi_Handler(void)
{
    puts("[Nmi_Handler] Unhandled exception occured!");
    while (1) continue;
}

void HardFault_Handler(void)
{
    puts("[HardFault_Handler] Unhandled exception occured!");
    while (1) continue;
}

void MemoryManage_Handler(void)
{
    puts("[MemoryManage_Handler] Unhandled exception occured!");
    while (1) continue;
}

void BusFault_Handler(void)
{
    puts("[BusFault_Handler] Unhandled exception occured!");
    while (1) continue;
}

void UsageFault_Handler(void)
{
    puts("[UsageFault_Handler] Unhandled exception occured!");
    while (1) continue;
}

void DebugMon_Handler(void)
{
    puts("[DebugMon_Handler] Unhandled exception occured!");
    while (1) continue;
}
