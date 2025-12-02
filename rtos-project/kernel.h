// Nicholas Nhat Tran
// 1002027150

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef KERNEL_H_
#define KERNEL_H_

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "shell.h"

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// function pointer
typedef void (*_fn)();

// mutex
#define MAX_MUTEXES 1
#define MAX_MUTEX_QUEUE_SIZE 2
#define resource 0

// semaphore
#define MAX_SEMAPHORES 3
#define MAX_SEMAPHORE_QUEUE_SIZE 2
#define keyPressed 0
#define keyReleased 1
#define flashReq 2

// tasks
#define MAX_TASKS 12

// task states
#define STATE_INVALID           0 // no task
#define STATE_UNRUN             1 // task has never been run
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_SEMAPHORE 4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_MUTEX     5 // has run, but now blocked by mutex
#define STATE_KILLED            6 // task has been killed

typedef struct _mutex
{
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;

typedef struct _semaphore
{
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;

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
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
} MutexInfo;

typedef struct _sem_info
{
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} SemaphoreInfo;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool initMutex(uint8_t mutex);
bool initSemaphore(uint8_t semaphore, uint8_t count);

void initRtos(void);
void startRtos(void);

bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes);
void killThread(_fn fn);
void destroyThread(uint32_t pid);
void restartThread(_fn fn);
void restartThreadKernel(_fn fn);
void setThreadPriority(_fn fn, uint8_t priority);

void yield(void);
void sleep(uint32_t tick);
void wait(int8_t semaphore);
void post(int8_t semaphore);
void lock(int8_t mutex);
void unlock(int8_t mutex);

void testSRAMpriv();
void testSRAMunpriv();
void testSRAMunprivFree();

void printPid(int newlines);

void systickIsr(void);
void pendSvIsr(void);
void triggerPendSvFault(void);
void svCallIsr(void);

bool populateTaskInfo(uint8_t index, TaskInfo *info);
bool getResourceInfo(uint8_t type, uint8_t index, void *info);
int32_t getPid(const char name[]);
void launchTask(const char name[]);
void setPreemption(bool on);
void setPriorityInheritance(bool on);
void setSched(bool prio_on);
uint8_t getTaskCurrent();
void forceKillThread(int taskIndex);

#endif
