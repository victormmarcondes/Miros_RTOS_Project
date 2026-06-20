/****************************************************************************
* MInimal Real-time Operating System (MiROS), GNU-ARM port.
*
* This software is a teaching aid to illustrate the concepts underlying
* a Real-Time Operating System (RTOS). The main goal of the software is
* simplicity and clear presentation of the concepts, but without dealing
* with various corner cases, portability, or error handling. For these
* reasons, the software is generally NOT intended or recommended for use
* in commercial applications.
*
* Copyright (C) 2018 Miro Samek. All Rights Reserved.
*
* SPDX-License-Identifier: GPL-3.0-or-later
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
* Git repo:
* https://github.com/QuantumLeaps/MiROS
****************************************************************************/
#include <cstdint>
#include <cstring>
#include "miros.h"
#include "qassert.h"
#include "stm32g4xx.h"
#include "stm32g4xx_it.h"
#include "SEGGER_SYSVIEW.h"

Q_DEFINE_THIS_FILE

namespace rtos {

    OSThread * volatile OS_curr; /* pointer to the current thread */
    OSThread * volatile OS_next; /* pointer to the next thread to run */

    OSThread *OS_thread[32 + 1]; /* array of threads started so far */
    uint32_t OS_readySet; /* bitmask of threads that are ready to run */

    uint8_t OS_threadNum; /* number of threads started */
    uint8_t OS_currIdx; /* current thread index for the circular array */

    OSThread idleThread;

    Semaphore::Semaphore(int32_t init)
    {
        token = init;
        blockedSet = 0U;
    }

    void Semaphore::lock()
        {
            __disable_irq();

            token--;

            if (token < 0)
            {
                SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)OS_currIdx, 0);

                blockedSet   |=  (1U << (OS_currIdx - 1U));
                OS_readySet  &= ~(1U << (OS_currIdx - 1U));

                OS_sched();
            }

            __enable_irq();
        }

    void Semaphore::unlock()
        {
            __disable_irq();

            token++;

            if (token <= 0)
            {
                for (uint8_t i = 1U; i < OS_threadNum; i++)
                {
                    if (blockedSet & (1U << (i - 1U)))
                    {
                        blockedSet  &= ~(1U << (i - 1U));
                        OS_readySet |=  (1U << (i - 1U));
                        SEGGER_SYSVIEW_OnTaskStartReady((unsigned)i);
                        break;
                    }
                }
                OS_sched();
            }

            __enable_irq();
        }

    void main_idleThread() {
        while (1) {                                             //thread de idle
            OS_onIdle();
        }
    }

    void OS_init(void *stkSto, uint32_t stkSize) {
        /* set the PendSV interrupt priority to the lowest level 0xFF */
        *(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16);                            //Pendsv?

        /* start idleThread thread */
        OSThread_start(&idleThread,                                                   //starta a thread de idle
                    &main_idleThread,
					254U,
                    stkSto, stkSize);
    }

    void OS_sched(void) {
            if (OS_readySet == 0U) { /* idle condition? */
                OS_currIdx = 0U; /* the idle thread */
                SEGGER_SYSVIEW_OnIdle();
            } else {
            	uint8_t aux = 0;
            	uint32_t aux_deadline = 0xFFFFFFFFU;
            	for(uint8_t i = 1; i < OS_threadNum; i++){
            		if((OS_readySet & (1U <<(i - 1U))) != 0 ){
            			if(OS_thread[i]->deadline < aux_deadline) aux = i;
            		}
            	}
            	OS_currIdx = aux;

            }
            OS_next = OS_thread[OS_currIdx];

            /* trigger PendSV, if needed */
            if(OS_next != OS_curr){                                                     //Caso nao consiga trocar a thread executada
                *(uint32_t volatile *)0xE000ED04 = (1U << 28);                          //ele chama o pendsv, para gerar a interrupcao e forcar a troca de contexto
            }
        }

    void OS_run(void) {                                                             
        /* callback to configure and start interrupts */
        OS_onStartup();

        __disable_irq();                                                                //interrompe a as requests de interrupcoes
        OS_sched();                                                                     //escalona
        __enable_irq();                                                                 //liga as interrupcoes de volta.
        
        /* the following code should never execute */
        Q_ERROR();

    }

    void OS_tick(void) {
    	uint32_t n = 0;
        for(n=1U;n<OS_threadNum; n++){ 				/* cycle through every thread but the idle */
            if(OS_thread[n]->timeout != 0U){
                OS_thread[n]->timeout--;			/* decrease the timeout */
                if(OS_thread[n]->timeout == 0U){
                    OS_readySet |= (1U << (n-1U));	/* if the thread is ready mask the corresponding bit */
                    SEGGER_SYSVIEW_OnTaskStartReady((unsigned)n);
                }
            }
        }
    }

    void OS_yield(){
    	__disable_irq();
    	OS_sched();
    	__enable_irq();
    }

    void OS_delay(uint32_t ticks){
           __asm volatile ("cpsid i");

           Q_REQUIRE(OS_curr != OS_thread[0]);

           OS_curr->timeout       = ticks;
           OS_curr->next_deadline += OS_curr->period; /* avança o deadline absoluto */

           OS_readySet &= ~(1U << (OS_currIdx - 1U));

           SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)OS_currIdx, 0);

           OS_sched();

           /* Se OS_next ainda é idle, registrar (pode ter mudado em OS_sched) */
           if (OS_next == OS_thread[0])
           {
               SEGGER_SYSVIEW_OnIdle();
           }

           __asm volatile ("cpsie i");
       }

    void OSThread_start(                                                                          //starta uma thread
        OSThread *me,
        OSThreadHandler threadHandler,
		uint32_t deadline,
        void *stkSto, uint32_t stkSize)
    {
        /* round down the stack top to the 8-byte boundary
        * NOTE: ARM Cortex-M stack grows down from hi -> low memory
        */
    	SEGGER_SYSVIEW_TASKINFO Info;

        uint32_t *sp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) * 8);                    //arredonda o valor do stack pra baixo
        uint32_t *stk_limit;                                                                    //define o limite do stack

        /* thread number must be in range
        * and must be unused
        */
        Q_REQUIRE((OS_threadNum < Q_DIM(OS_thread)) && (OS_thread[OS_threadNum] == (OSThread *)0));              //require pede que tenha espaco pra uma thread adicional e nao esteja ocupado por outra thread

        *(--sp) = (1U << 24);  /* xPSR */                                    //o stack de memoria do arm comeca ao contrario (enderecos)
        *(--sp) = (uint32_t)threadHandler; /* PC */                          //ele comeca no fim e vai indo pro comeco
        *(--sp) = 0x0000000EU; /* LR  */                                     //a cada --sp ele ta olhando pra uma posicao anterior do stack
        *(--sp) = 0x0000000CU; /* R12 */                                     //e salvando valores la. o que os valores significam exatamente?
        *(--sp) = 0x00000003U; /* R3  */
        *(--sp) = 0x00000002U; /* R2  */
        *(--sp) = 0x00000001U; /* R1  */
        *(--sp) = 0x00000000U; /* R0  */
        /* additionally, fake registers R4-R11 */
        *(--sp) = 0x0000000BU; /* R11 */
        *(--sp) = 0x0000000AU; /* R10 */
        *(--sp) = 0x00000009U; /* R9 */
        *(--sp) = 0x00000008U; /* R8 */
        *(--sp) = 0x00000007U; /* R7 */
        *(--sp) = 0x00000006U; /* R6 */
        *(--sp) = 0x00000005U; /* R5 */
        *(--sp) = 0x00000004U; /* R4 */

        /* save the top of the stack in the thread's attibute */
        me->sp = sp;                                                          //atribui o stack a thread

        me->period = deadline;      // por enquanto usar o mesmo valor
        me->next_deadline = deadline;
        me->deadline = deadline;
        me->timeout = 0U;

        /* round up the bottom of the stack to the 8-byte boundary */
        stk_limit = (uint32_t *)(((((uint32_t)stkSto - 1U) / 8) + 1U) * 8);                  //arredonda o limite para cima. O limite fica para baixo, como foi explicado anteriormente
                                                    
        /* pre-fill the unused part of the stack with 0xDEADBEEF */
        for (sp = sp - 1U; sp >= stk_limit; --sp) {                                          //define tudo o que nao foi usado com DEADBEEF (convencao meme)
            *sp = 0xDEADBEEFU;
        }

        /* register the thread with the OS */
        OS_thread[OS_threadNum] = me;
        OS_threadNum++;

        //atualiza o vetor de threads
        /* make the thread ready to run */

        if (OS_threadNum > 0U) {
            OS_readySet |= (1U << (OS_threadNum - 1U));
        }

        SEGGER_SYSVIEW_OnTaskCreate((unsigned)OS_threadNum - 1U);
        memset(&Info, 0, sizeof(Info));

        //
         Info.TaskID = (U32)(OS_threadNum - 1U);

         //Info.sName = Name;
         //Info.Prio = Priority;
         Info.StackBase = (U32)stkSto;
         Info.StackSize = stkSize;
         SEGGER_SYSVIEW_SendTaskInfo(&Info);
    }
    /***********************************************/
    void OS_onStartup(void) {
        // 1. O código chega aqui?
        // Coloque um breakpoint na linha de baixo.
        SystemCoreClock = HAL_RCC_GetSysClockFreq();

        // ARMADILHA 1: O clock retornou zero? (Falta de inicialização da HAL)
        if (SystemCoreClock == 0) {
            while(1);
        }

        // Tenta ligar o SysTick
        uint32_t erro = SysTick_Config(SystemCoreClock / TICKS_PER_SEC);

        // ARMADILHA 2: O valor estourou os 24 bits de novo?
        if (erro != 0) {
            while(1);
        }

        NVIC_SetPriority(PendSV_IRQn, 0xFFU);
        NVIC_SetPriority(SysTick_IRQn, 0x00);
    }

    void OS_onIdle(void) {
    #ifdef NDBEBUG
        __WFI(); /* stop the CPU and Wait for Interrupt */
    #endif
    }

}//fim namespace

void Q_onAssert(char const *module, int loc) {
    /* TBD: damage control */
    (void)module; /* avoid the "unused parameter" compiler warning */
    (void)loc;    /* avoid the "unused parameter" compiler warning */
    NVIC_SystemReset();
}

/***********************************************/
extern "C"
__attribute__ ((naked, optimize("-fno-stack-protector")))
void PendSV_Handler(void) {
__asm volatile (

    /* __disable_irq(); */
    "  CPSID         I                 \n"                 //desabilita as interrupcoes

    /* if (OS_curr != (OSThread *)0) { */
    "  LDR           r1,=_ZN4rtos7OS_currE       \n"       //salva a thread atual em r1
    "  LDR           r1,[r1,#0x00]     \n"                 //salva o endereco de memoria da thread em r1
    "  CBZ           r1,PendSV_restore \n"                 //se o registrador for zero chama a pendsv_restore

    /*     push registers r4-r11 on the stack */
    "  PUSH          {r4-r11}          \n"                 //empurra os valores de r4 ate r11 no stack

    /*     OS_curr->sp = sp; */
    "  LDR           r1,=_ZN4rtos7OS_currE       \n"         
    "  LDR           r1,[r1,#0x00]     \n"                 //salva o primeiro endereco de memoria da variavel 
    "  STR           sp,[r1,#0x00]     \n"                 //grava o valor do endereco de memoria apontado por r1 no sp
    /* } */

    "PendSV_restore:                   \n"                 //funcao
    /* sp = OS_next->sp; */
    "  LDR           r1,=_ZN4rtos7OS_nextE       \n"       //salva a proxima thread no r1
    "  LDR           r1,[r1,#0x00]     \n"                 //salva o endereco de memoria da prox thread no r1
    "  LDR           sp,[r1,#0x00]     \n"                 //grava o valor do endereco de memoria apontado por r1 no sp

    /* OS_curr = OS_next; */
    "  LDR           r1,=_ZN4rtos7OS_nextE       \n"       //salva a proxima thread no r1
    "  LDR           r1,[r1,#0x00]     \n"                 //salva o endereco de memoria da prox thread no r1
    "  LDR           r2,=_ZN4rtos7OS_currE       \n"       //salva a thread atual em r2
    "  STR           r1,[r2,#0x00]     \n"                 //salva o endereco de r2 em r1

    /* pop registers r4-r11 */
    "  POP           {r4-r11}          \n"                  //restaura o valor dos registradores falsos

    /* __enable_irq(); */
    "  CPSIE         I                 \n"                  //habilita as interrupcoes

    /* return to the next thread */
    "  BX            lr                \n"                  //return
    );
}
