/*
 * miros.h
 *
 *  Created on: Feb 6, 2025
 *      Author: guilh
 */

#ifndef INC_MIROS_H_
#define INC_MIROS_H_

#include "SEGGER.h"
#include "SEGGER_SYSVIEW.h"


namespace rtos {
/* Thread Control Block (TCB) */
typedef struct {
	uint32_t *sp; /* stack pointer */
    uint32_t timeout; /* timeout delay down-counter */
    uint32_t period;
    uint32_t deadline;
    uint32_t next_deadline;
    /* ... other attributes associated with a thread */
} OSThread;

class TCB{
    private:
    uint8_t numThreads;
    

};

class Semaphore {
private:

    int32_t token;

    uint32_t blockedSet;

public:

    Semaphore(int32_t init);

    void lock();
    void unlock();
};

const uint16_t TICKS_PER_SEC = 100U;

typedef void (*OSThreadHandler)();

void OS_init(void *stkSto, uint32_t stkSize);

/* callback to handle the idle condition */
void OS_onIdle(void);

/* this function must be called with interrupts DISABLED */
void OS_sched(void);

/* transfer control to the RTOS to run the threads */
void OS_run(void);

/* blocking delay */
void OS_delay(uint32_t ticks);

void OS_yield(void);

/* process all timeouts */
void OS_tick(void);

/* callback to configure and start interrupts */
void OS_onStartup(void);

void OSThread_start(
    OSThread *me,
    OSThreadHandler threadHandler,
	uint32_t deadline,
    void *stkSto, uint32_t stkSize);

}

#endif /* INC_MIROS_H_ */
