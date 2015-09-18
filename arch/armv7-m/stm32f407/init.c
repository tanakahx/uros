#include "stm32f4xx.h"

#define PLL_M 8
#define PLL_N 336
#define PLL_P 2
#define PLL_Q 7

static void EnablePll()
{
    /* Set up SYSCLK to 168 MHz */
    
    /* Use HSE for PLL source */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY))
        continue;
    
    /* Power supply setup */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    /*
     * AHB  = SYSCLK / 1 = 168 MHz
     * APB2 = SYSCLK / 2 = 84 MHz
     * APB1 = SYSCLK / 4 = 42 MHz
     */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1  |
                 RCC_CFGR_PPRE2_DIV2 |
                 RCC_CFGR_PPRE1_DIV4;
    
    /* * PLLCLK = HSI(16 MHz) / M * N / P */
    RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) -1) << 16) |
                   RCC_PLLCFGR_PLLSRC_HSE | (PLL_Q << 24);

    /* PLL ON */
    RCC->CR |= RCC_CR_PLLON;

    /* Wait until PLL is locked up */
    while (!(RCC->CR & RCC_CR_PLLRDY))
        continue;

    /* Enable I-Cache and D-Cache and flash latency */
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;

    /* Switch SYSCLK source to PLLCLK */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    /* Ensure the register updated */
    while ((RCC->CFGR & RCC_CFGR_SW) != RCC_CFGR_SW_PLL)
        continue;
}

void system_init(void)
{
    SystemInit();
    
    EnablePll();

    /* Enable clock for each peripherals */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PC12~PC15 pin is assigned for GPIO output */
    GPIOD->MODER = 0x55550000;
    GPIOD->ODR   = 0x0000F000;

}
