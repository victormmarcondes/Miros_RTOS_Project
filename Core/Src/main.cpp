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
#include "VL53L4CD_api.h"

#define TASK_PERIOD 40U
#define BUFFER_SIZE 10U

static uint32_t buffer_fifo[BUFFER_SIZE];
static volatile uint32_t buffer_head = 0U;
static volatile uint32_t buffer_tail = 0U;

rtos::Semaphore sem_mutex(1);
rtos::Semaphore sem_espacos(BUFFER_SIZE);
rtos::Semaphore sem_itens(0);

rtos::Semaphore sem_setpoint(1);

volatile uint32_t prod = 100U;

uint8_t data_ready = 0;

rtos::Semaphore sem(1);

uint32_t stack_produtor1[256];
rtos::OSThread produtor1;

void main_produtor1(){
	sem_espacos.lock();
	sem_mutex.lock();

	buffer_fifo[buffer_head] = prod + 10U;
	buffer_head = (buffer_head + 1U) % BUFFER_SIZE;
	prod = (prod + 10U <= 200U) ? prod + 10U : 100U;

	sem_mutex.unlock();
	sem_itens.unlock();
}

uint32_t stack_produtor2[256];
rtos::OSThread produtor2;

void main_produtor2() {
    sem_espacos.lock();
    sem_mutex.lock();

    buffer_fifo[buffer_head] = prod + 15U;
    buffer_head = (buffer_head + 1U) % BUFFER_SIZE;
    prod = (prod + 15U <= 200U) ? prod + 15U : 100U;

    sem_mutex.unlock();
    sem_itens.unlock();
}

uint32_t stack_consumidor[256];
rtos::OSThread consumidor;

void main_consumidor() {
    sem_itens.lock();
    sem_mutex.lock();

    uint32_t item   = buffer_fifo[buffer_tail];
    buffer_tail     = (buffer_tail + 1U) % BUFFER_SIZE;
    prod            = (item >= 10U) ? item - 10U : 0U;

    sem_mutex.unlock();
    sem_espacos.unlock();
}


void task_muda_setpoint() {
    sem_setpoint.lock();
    Controle::SetSetPoint(50.0f);
    sem_setpoint.unlock();
}

uint32_t stack_sensor[256 * 4];
rtos::OSThread sensor;

void thread_sensor() {
    static bool sensor_init_done = false;
    if (!sensor_init_done) {
        Controle::init();          /* VL53L4CD_SensorInit + StartRanging */
        sensor_init_done = true;
    }
    Controle::TaskLeitor();        /*ClearInterrupt */
}

uint32_t stack_controle[256];
rtos::OSThread controle;

void thread_controle() {
    Controle::TaskControle();
}

uint32_t stack_atuador[256];
rtos::OSThread atuador;

void thread_atuador() {
    static bool       tim_init_done = false;
    static TIM_HandleTypeDef htim1  = {0};

    if (!tim_init_done) {
        __HAL_RCC_TIM1_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /* ---- Configura TIM1 para PWM ---- */
        htim1.Instance               = TIM1;
        htim1.Init.Prescaler         = 83;          /* 170 MHz / 84 ≈ 2.024 MHz */
        htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
        htim1.Init.Period            = 999;          /* ≈ 2 kHz PWM              */
        htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
        htim1.Init.RepetitionCounter = 0;
        HAL_TIM_PWM_Init(&htim1);

        TIM_OC_InitTypeDef oc = {0};
        oc.OCMode       = TIM_OCMODE_PWM1;
        oc.Pulse        = 0;                         /* duty inicial = 0 %       */
        oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
        oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
        oc.OCFastMode   = TIM_OCFAST_DISABLE;
        oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
        oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
        HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1);

        /* ---- Configura GPIO PC0 em Alternate Function para TIM1_CH1 ---- */
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_0;
        gpio.Mode      = GPIO_MODE_AF_PP;    /* obrigatório para PWM */
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF2_TIM1;      /* TIM1_CH1 no STM32G474 */
        HAL_GPIO_Init(GPIOC, &gpio);

        HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

        /* TIM1 é Advanced Timer: MOE obrigatório para habilitar saída */
        TIM1->BDTR |= TIM_BDTR_MOE;

        tim_init_done = true;
    }

    /* Atualiza duty cycle com tensão calculada pelo controlador */
    uint32_t cmp = (uint32_t)((Controle::GetTension() / 4.0f) * 999.0f);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, cmp);
}

void EXTI15_10_IRQHandler(void) {
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);  /* limpa flag para evitar reset */

    static rtos::AperiodicTask task;
    task.Handler    = &task_muda_setpoint;
    task.parametros = nullptr;

    rtos::APTask = &task;
    rtos::OS_readySet |= 1U;               /* ativa Deferrable Server (OS_thread[1]) */
}

uint32_t stack_idleThread[256];

int main(void){
	__disable_irq();
	HAL_Init();
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
    /* Cria a thread idle */
    rtos::OS_init(stack_idleThread, sizeof(stack_idleThread));

    // prod's consumidor
    rtos::OSThread_start(&consumidor, "Consumidor", &main_consumidor,
                             10U, stack_consumidor, sizeof(stack_consumidor));

    rtos::OSThread_start(&produtor1, "Prod_1", &main_produtor1,
                             15U, stack_produtor1, sizeof(stack_produtor1));

    rtos::OSThread_start(&produtor2, "Prod_2", &main_produtor2,
                             20U, stack_produtor2, sizeof(stack_produtor2));


    /*
    // controle
    rtos::OSThread_start(&sensor, "sensor", &thread_sensor,
                             TASK_PERIOD, stack_sensor, sizeof(stack_sensor));

    rtos::OSThread_start(&controle, "controle", &thread_controle,
                             TASK_PERIOD, stack_controle, sizeof(stack_controle));

    rtos::OSThread_start(&atuador, "atuador", &thread_atuador,
                             TASK_PERIOD, stack_atuador, sizeof(stack_atuador));*/




    __enable_irq();
    rtos::OS_run();
}


