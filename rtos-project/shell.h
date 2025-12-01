// Nicholas Nhat Tran
// 1002027150

// Shell functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

#include <stdbool.h>

typedef struct _task_info
{
    uint32_t pid;
    char name[16];
    uint8_t state;
    uint8_t priority;
    uint32_t currentPriority;
    uint32_t time;
    uint32_t totalTime;
    uint32_t ticks;
} TaskInfo;

typedef struct _mutex_info
{
    bool lock;
    uint8_t lockedBy;
    uint8_t queueSize;
} MutexInfo;

typedef struct _sem_info
{
    uint8_t count;
    uint8_t queueSize;
} SemaphoreInfo;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void yield(void);
void reboot(void);
void ps(void);
void ipcs(void);
void kill(uint32_t pid);
void pkill(const char name[]);
void pi(bool on);
void preempt(bool on);
void sched(bool prio_on);
void pidof(const char name[]);
void run(const char name[]);
void shell(void);

#endif
