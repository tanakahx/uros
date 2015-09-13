#include "stdtype.h"

extern unsigned int stack_bottom;

void Reset_Handler() __attribute__((weak));
void NMI_Handler() __attribute__((weak));
void HardFault_Handler() __attribute__((weak));
void MemManage_Handler() __attribute__((weak));
void BusFault_Handler() __attribute__((weak));
void UsageFault_Handler() __attribute__((weak));
void SVC_Handler() __attribute__((weak));
void DebugMon_Handler() __attribute__((weak));
void PendSV_Handler() __attribute__((weak));
void SysTick_Handler() __attribute__((weak));
void Uart0_Handler() __attribute__((weak));
  
void (* const vector_table[])()  = {
    (void (*)())&stack_bottom,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    NULL,
    NULL,
    NULL,
    NULL,
    SVC_Handler,
    DebugMon_Handler,
    NULL,
    PendSV_Handler,
    SysTick_Handler,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    Uart0_Handler,
};
