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
#include "controle.h"
#include <cstdint>
#include "miros.h"
#include "SEGGER_SYSVIEW.h"
#include "SEGGER_RTT.h"

volatile uint32_t conta0 = 0U;
volatile uint32_t conta1 = 0U;
volatile uint32_t conta2 = 0U;
volatile uint32_t prod = 100U;

rtos::Semaphore sem(1);

uint32_t stack_produtor1[256];
rtos::OSThread produtor1;

void main_produtor1(){
	while(1){
		sem.lock();
		if(prod <= 200U){
			prod = prod + 10;
		}
		sem.unlock();

		rtos::OS_delay(15U);
	}
}

uint32_t stack_produtor2[256];
rtos::OSThread produtor2;

void main_produtor2(){
	while(1){
		sem.lock();
		if(prod <= 200U){
			prod = prod + 15;
		}
		sem.unlock();

		rtos::OS_delay(20U);
	}
}

uint32_t stack_consumidor[256];
rtos::OSThread consumidor;

void main_consumidor(){
	while(1){
		sem.lock();
		prod = prod - 10;
		sem.unlock();

		rtos::OS_delay(10U);
	}
}


uint32_t stack_blinky1[256];
rtos::OSThread blinky1;

void main_blinky1(){
    while (1){
        conta0 = conta0 + 1;

        rtos::OS_delay(10U);
    }
}

uint32_t stack_blinky2[256];
rtos::OSThread blinky2;

void main_blinky2(){
    while (1){
    	conta1 = conta1 + 1;

        rtos::OS_delay(20U);
    }
}

uint32_t stack_blinky3[256];
rtos::OSThread blinky3;

void main_blinky3(){
    while (1){
    	conta2 = conta2 + 1;

        rtos::OS_delay(25U);
    }
}

uint32_t stack_aperiodic[256];
rtos::OSThread aperiodic;

void aperiodic_server(){             //deadline 40s
    while(1){
        if(rtos::click > 0){
            Controle::SetSetPoint(rtos::click * 100);
            rtos::click = 0;
        }
        rtos::OS_delay(10);
    }
}

uint32_t stack_controle[256];
rtos::OSThread controle;

void thread_controle(){

	__HAL_RCC_TIM1_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

    TIM_HandleTypeDef htim1;
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 83;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 999;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    HAL_TIM_PWM_Init(&htim1);

    TIM_OC_InitTypeDef config;

    config.OCMode = TIM_OCMODE_PWM1;
    config.Pulse = 500;
    config.OCPolarity = TIM_OCPOLARITY_HIGH;
    config.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim1, &config, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

    GPIO_InitTypeDef GPIO;

    GPIO.Pin = GPIO_PIN_0;
    GPIO.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO.Pull = GPIO_NOPULL;
    GPIO.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO.Alternate = GPIO_AF1_TIM15;



    HAL_GPIO_Init(GPIOC, &GPIO);

    while(1){
        int pulse = Controle::TaskControle();

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);

        rtos::OS_delay(40U);
    }
}

uint32_t stack_sensor[256];
rtos::OSThread sensor;

void thread_sensor(){
    while(1){

        rtos::OS_delay(30U);
    }
}

uint32_t stack_idleThread[256];

int main(void){
	__disable_irq();
	HAL_Init();
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
    /* Cria a thread idle */
    rtos::OS_init(stack_idleThread, sizeof(stack_idleThread));

    /* Cria thread blinky1 — deadline/período = 100 ticks */
   /* rtos::OSThread_start(&blinky1,
                         "Blinky1",
                         &main_blinky1,
                         80U,
                         stack_blinky1,
                         sizeof(stack_blinky1));

    /* Cria thread blinky2 — deadline/período = 20 ticks 
    rtos::OSThread_start(&blinky2,"Blinky2",
                         &main_blinky2,
                         20U,
                         stack_blinky2, sizeof(stack_blinky2));

    /* Cria thread blinky3 — deadline/período = 50 ticks 
    rtos::OSThread_start(&blinky3,"Blinky3",
                         &main_blinky3,
                         40U,
                         stack_blinky3, sizeof(stack_blinky3));

    rtos::OSThread_start(&consumidor,"Consumidor",
                             &main_consumidor,
                             10U,
                             stack_consumidor, sizeof(stack_consumidor));

    rtos::OSThread_start(&produtor1,"Prod_1",
                                 &main_produtor1,
                                 15U,
                                 stack_produtor1, sizeof(stack_produtor1));

    rtos::OSThread_start(&produtor2,"Prod_2",
                                     &main_produtor2,
                                     20U,
                                     stack_produtor2, sizeof(stack_produtor2)); */

    rtos::OSThread_start(&aperiodic,"click",
                                     &aperiodic_server,
                                     5U,
                                     stack_aperiodic, sizeof(stack_aperiodic));

    rtos::OSThread_start(&controle,"controle",
                                         &thread_controle,
                                         10U,
                                         stack_controle, sizeof(stack_controle));

    rtos::OSThread_start(&sensor,"sensor",
                                         &thread_sensor,
                                         10U,
                                         stack_sensor, sizeof(stack_sensor));


    __enable_irq();

    rtos::OS_run();

}


