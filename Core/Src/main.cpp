/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */


#include "main.h"
#include <cstdint>
#include "miros.h"
#include "SEGGER_SYSVIEW.h"


volatile uint32_t conta0 = 0U;
volatile uint32_t conta1 = 0U;
volatile uint32_t conta2 = 0U;

rtos::Semaphore sem(0);

uint32_t stack_blinky1[256];
rtos::OSThread blinky1;

void main_blinky1()
{
    while (1)
    {
        sem.lock();

        conta0 = conta0 + 1;


        rtos::OS_delay(10U);
    }
}

uint32_t stack_blinky2[256];
rtos::OSThread blinky2;

void main_blinky2()
{
    while (1)
    {
    	 conta1 = conta1 + 1;

        sem.unlock();

        rtos::OS_delay(20U);
    }
}

uint32_t stack_blinky3[256];
rtos::OSThread blinky3;

void main_blinky3()
{
    while (1)
    {
    	 conta2 = conta2 + 1;

        rtos::OS_delay(25U);
    }
}

uint32_t stack_idleThread[256];

int main(void)
{
	HAL_Init();
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();

    /* Cria a thread idle */
    rtos::OS_init(stack_idleThread, sizeof(stack_idleThread));

    /* Cria thread blinky1 — deadline/período = 100 ticks */
    rtos::OSThread_start(&blinky1,
                         &main_blinky1,
                         100U,
                         stack_blinky1, sizeof(stack_blinky1));

    /* Cria thread blinky2 — deadline/período = 20 ticks */
    rtos::OSThread_start(&blinky2,
                         &main_blinky2,
                         20U,
                         stack_blinky2, sizeof(stack_blinky2));

    /* Cria thread blinky3 — deadline/período = 50 ticks */
    rtos::OSThread_start(&blinky3,
                         &main_blinky3,
                         50U,
                         stack_blinky3, sizeof(stack_blinky3));

    rtos::OS_run();

}


