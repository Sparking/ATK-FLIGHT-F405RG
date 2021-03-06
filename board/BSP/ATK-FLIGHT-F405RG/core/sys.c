﻿#include <FreeRTOS.h>
#include <task.h>
#include <core/sys.h>
#include <periph/pinmux.h>
#include <periph/usart-uart.h>
#include <periph/spi.h>
#include <periph/gpio.h>
#include <driver/mpu6000.h>

extern void xPortPendSVHandler(void) __attribute__ (( naked ));
extern void xPortSysTickHandler(void);
void vPortSVCHandler(void) __attribute__ (( naked ));

void delay_us(unsigned int nus)
{
    unsigned int t[2], all_counts, current_counts;

    t[0] = SysTick->VAL;
    for (all_counts = nus * 80, current_counts = 0; current_counts < all_counts; t[0] = t[1]) {
        t[1] = SysTick->VAL;
        if (t[1] > t[0]) {
            current_counts = current_counts + t[0] + SysTick->LOAD - t[1];
        } else {
            current_counts = current_counts + t[0] - t[1];
		}
    }
}

void delay_ms(unsigned int nms)
{
    while (nms-- > 0) {
        delay_us(1000);
	}
}

#define FAC_MS (1000 / configTICK_RATE_HZ)
void delay_xms(unsigned int ms)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        if (ms >= FAC_MS) {
            vTaskDelay(ms / FAC_MS);
        }

        ms %= FAC_MS;
    }
    delay_us(ms * 1000);
}

unsigned int get_systick_count(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return xTaskGetTickCount();
	}

    return HAL_GetTick();
}

void SysTick_Handler(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
    HAL_IncTick();
}

void Error_Handler(void)
{
    while (1) {
        gpio_set_pin(GPIOB, GPIO_PIN_9);
        delay_ms(1000);
        gpio_reset_pin(GPIOB, GPIO_PIN_9);
        delay_ms(1000);
        err("error!!!\r\n");
	}
}

void system_clock_init(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();
  
    /* The voltage scaling allows optimizing the power consumption when the device is 
       clocked below the maximum system frequency, to update the voltage scaling value 
       regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
  
    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
       clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
    if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported  */
    if (HAL_GetREVID() == 0x1001) {
        /* Enable the Flash prefetch */
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }
}

void system_init(void)
{
    SCB->SHCSR |= 0x00007000;;
    HAL_Init();
    system_clock_init();
    pinmux_init();
    uart_init();
    spi_init();
    mpu6000_init();
    periph_interrupt_config();
}

void HardFault_Handler(void)
{
    __asm volatile(
        "tst    lr, #4      \n"
        "ite    eq          \n"
        "mrseq  r0, msp     \n"
        "mrsne  r0, psp     \n"
        "b      HardFault_Handler_CallBack\n"
    );
}

void HardFault_Handler_CallBack(unsigned int *args)
{
    unsigned int stacked_r0;
    unsigned int stacked_r1;
    unsigned int stacked_r2;
    unsigned int stacked_r3;
    unsigned int stacked_r12;
    unsigned int stacked_lr;
    unsigned int stacked_pc;
    unsigned int stacked_psr;

    stacked_r0  = args[0];
    stacked_r1  = args[1];
    stacked_r2  = args[2];
    stacked_r3  = args[3];
    stacked_r12 = args[4];
    stacked_lr  = args[5];
    stacked_pc  = args[6];
    stacked_psr = args[7];
    printf("\r\n[Hard fault handler - all numbers in hex]\r\n");
    printf("r0   :  %X\r\n", stacked_r0);
    printf("r1   :  %X\r\n", stacked_r1);
    printf("r2   :  %X\r\n", stacked_r2);
    printf("r3   :  %X\r\n", stacked_r3);
    printf("r12  :  %X\r\n", stacked_r12);
    printf("lr   :  %X\r\n", stacked_lr);
    printf("pc   :  %X\r\n", stacked_pc);
    printf("psr  :  %X\r\n", stacked_psr);
    printf("BFAR :  %lX\r\n", SCB->BFAR);
    printf("CFSR :  %lX\r\n", SCB->CFSR);
    printf("HFSR :  %lX\r\n", SCB->HFSR);
    printf("DFSR :  %lX\r\n", SCB->DFSR);
    printf("AFSR :  %lX\r\n", SCB->AFSR);
    printf("SHCSR : %lX\r\n", SCB->SHCSR);
    while (1) {
        gpio_set_pin(GPIOB, GPIO_PIN_9);
        delay_ms(1000);
        gpio_reset_pin(GPIOB, GPIO_PIN_9);
        delay_ms(1000);
    }
}
