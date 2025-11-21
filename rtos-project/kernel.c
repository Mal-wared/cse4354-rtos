// Kernel functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "mm.h"
#include "kernel.h"
#include "uart0.h"
#include "util.h"
#include "faults.h"
#include "stackHelper.h"
#include "tasks.h"

uint32_t pid = 0;
uint64_t mask;

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// mutex
typedef struct _mutex
{
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;
mutex mutexes[MAX_MUTEXES];

// semaphore
typedef struct _semaphore
{
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;
semaphore semaphores[MAX_SEMAPHORES];

// task states
#define STATE_INVALID           0 // no task
#define STATE_UNRUN             1 // task has never been run
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_SEMAPHORE 4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_MUTEX     5 // has run, but now blocked by mutex
#define STATE_KILLED            6 // task has been killed

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks

// control
bool priorityScheduler = true;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = false;          // preemption (true) or cooperative (false)

// tcb
#define NUM_PRIORITIES   8
struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;              // used to uniquely identify thread (add of task fn)
    void *sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint64_t srd;                  // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    uint8_t mutex;           // index of the mutex in use or blocking the thread
    uint8_t semaphore;     // index of the semaphore that is blocking the thread
} tcb[MAX_TASKS];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool initMutex(uint8_t mutex)
{
    bool ok = (mutex < MAX_MUTEXES);
    if (ok)
    {
        mutexes[mutex].lock = false;
        mutexes[mutex].lockedBy = 0;
    }
    return ok;
}

bool initSemaphore(uint8_t semaphore, uint8_t count)
{
    bool ok = (semaphore < MAX_SEMAPHORES);
    {
        semaphores[semaphore].count = count;
    }
    return ok;
}

// REQUIRED: initialize systick for 1ms system timer
void initRtos(void)
{
    NVIC_ST_RELOAD_R = 39999;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_ENABLE | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_CLK_SRC;

    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
    }
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void)
{
    bool ok;
    static uint8_t task = 0xFF;
    ok = false;
    while (!ok)
    {
        task++;
        if (task >= MAX_TASKS)
            task = 0;
        ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
    }
    return task;
}


// REQUIRED: modify this function to start the operating system
// by calling scheduler, set srd bits, setting PSP, ASP bit, call fn with fn add in R0
// fn set TMPL bit, and PC <= fn
void startRtos(void)
{
    taskCurrent = rtosScheduler();
    tcb[taskCurrent].state = STATE_READY;

    // apply MPU settings to task
    applySramAccessMask(tcb[taskCurrent].srd);

    uint32_t* psp = tcb[taskCurrent].sp;
    setPsp(psp);
    loadR3((uint32_t)tcb[taskCurrent].pid);
    setAspBit();
    setTMPL();
    setPC();
}

// REQUIRED:
// add task if room in task list
// store the thread name
// allocate stack space and store top of stack in sp and spInit
// set the srd bits based on the memory allocation
bool createThread(_fn fn, const char name[], uint8_t priority,
                  uint32_t stackBytes)
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
    if (taskCount < MAX_TASKS)
    {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS))
        {
            found = (tcb[i++].pid == fn);
        }
        if (!found)
        {
            // find first available tcb record
            i = 0;
            while (tcb[i].state != STATE_INVALID)
            {
                i++;
                if (i >= MAX_TASKS) {
                    // No available space in the tcb
                    return false;
                }
            }

            // allocate memory for the process
            void *stack = mallocHeap(stackBytes);
            if (stack == NULL) {
                return false;
            }

            // set srd bits for tcb
            tcb[i].srd = createNoSramAccessMask();
            addSramAccessWindow(&tcb[i].srd, (uint32_t*)stack, stackBytes);

            // set stack pointer dummy variables
            uint32_t* sp = (uint32_t*)((uint32_t)stack + stackBytes);
            *(--sp) = 0x01000000;     // xPSR
            *(--sp) = (uint32_t)fn;  // PC
            *(--sp) = 0xFFFFFFFD;     // LR
            *(--sp) = 0x12121212;     // R12
            *(--sp) = 0x03030303;     // R3
            *(--sp) = 0x02020202;     // R2
            *(--sp) = 0x01010101;     // R1
            *(--sp) = 0x00000000;     // R0
            *(--sp) = 0x11111111;     // R11
            *(--sp) = 0x10101010;     // R10
            *(--sp) = 0x09090909;     // R9
            *(--sp) = 0x08080808;     // R8
            *(--sp) = 0x07070707;     // R7
            *(--sp) = 0x06060606;     // R6
            *(--sp) = 0x05050505;     // R5
            *(--sp) = 0x04040404;     // R4
            tcb[i].sp = sp;
            // set tcb properties
            tcb[i].state = STATE_UNRUN;
            tcb[i].pid = fn;
            strncpy(tcb[i].name, name, sizeof(tcb[i].name));
            tcb[i].priority = priority;
            tcb[i].currentPriority = priority;


            // increment task count
            taskCount++;
            ok = true;
        }
    }
    return ok;
}

// REQUIRED: modify this function to kill a thread
// REQUIRED: free memory, remove any pending semaphore waiting,
//           unlock any mutexes, mark state as killed
void killThread(_fn fn)
{
}

// REQUIRED: modify this function to restart a thread, including creating a stack
void restartThread(_fn fn)
{
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield(void)
{
    __asm(" SVC #0 ");
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm(" SVC #1 ");
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{

}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
}

// REQUIRED: modify this function to lock a mutex using pendsv
void lock(int8_t mutex)
{
    __asm(" SVC #2 ");
}

// REQUIRED: modify this function to unlock a mutex using pendsv
void unlock(int8_t mutex)
{
}

void testSRAMpriv()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = {512, 1024, 512, 1536, 1024, 1024, 1024, 1024, 512, 4096};
    int i;
    for (i = 0; i < 10; i++) {
        pointers[i] = mallocHeap(malloc_regions[i]);
    }
    applySramAccessMask(mask);
    *pointers[2] = 10;

    for (i = 0; i < 10; i++) {
        freeHeap(pointers[i]);
    }


}

// can't free heap in unprivileged mode
void testSRAMunpriv()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = {512, 1024, 512, 1536, 1024, 1024, 1024, 1024, 512, 4096};
    int i;
    for (i = 0; i < 10; i++) {
        pointers[i] = mallocHeap(malloc_regions[i]);
    }
    applySramAccessMask(mask);
    setTMPL();
    *pointers[2] = 10;
}

void testSRAMunprivFree()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = {512, 1024, 512, 1536, 1024, 1024, 1024, 1024, 512, 4096};
    int i;
    for (i = 0; i < 10; i++) {
        pointers[i] = mallocHeap(malloc_regions[i]);
    }
    freeHeap(pointers[2]);
    applySramAccessMask(mask);
    setTMPL();
    *pointers[2] = 10;

}

void printPid(int newlines)
{
    // String of size 12 for 10 digits (4,294,967,295) + 1 sign + 1 null terminator
    char pidStr[12];
    itoa(pid, pidStr);
    putsUart0(pidStr);
    int i;
    for (i = 0; i < newlines; i++)
    {
        putsUart0("\n");
    }
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr(void)
{
    int i = 0;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (tcb[i].state == STATE_DELAYED)
        {
            tcb[i].ticks--;
            if (tcb[i].ticks == 0)
            {
                tcb[i].state = STATE_READY;
            }
        }
    }
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr(void)
{
    tcb[taskCurrent].sp = saveContext();

    putsUart0("--- PENDSV HANDLER ---\n");
    putsUart0("Pendsv in process ");
    printPid(0);

    if (NVIC_FAULT_STAT_R & (NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR))
    {
        NVIC_FAULT_STAT_R &= ~(NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR);
        putsUart0(", called from MPU\n\n");
    }
    else
    {
        putsUart0("\n\n");
    }

    uint32_t debugFlags = PRINT_MFAULT_FLAGS;
    printFaultDebug(debugFlags);


    taskCurrent = rtosScheduler();
    applySramAccessMask(tcb[taskCurrent].srd);
    restoreContext(tcb[taskCurrent].sp);

}

void triggerPendSvFault()
{
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}



// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr(void)
{
    uint32_t* psp = getPsp();
    uint32_t* stacked_pc = (uint32_t*)psp[6];
    uint8_t* pc = (uint8_t*)stacked_pc;
    pc = pc - 2;
    uint8_t svcCallNum = *pc;

    switch(svcCallNum) {
        case 0:
            triggerPendSvFault();
            break;
        case 1:
            tcb[taskCurrent].state = STATE_DELAYED;
            tcb[taskCurrent].ticks = psp[0];
            triggerPendSvFault();
            break;
        case 2:
            if (mutexes[psp[0]].lock)
            {
                mutexes[psp[0]].processQueue[mutexes[psp[0]].queueSize] = taskCurrent;
                mutexes[psp[0]].queueSize++;
                tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;
                tcb[taskCurrent].mutex = psp[0];
                triggerPendSvFault();
            }
            else
            {
                mutexes[psp[0]].lockedBy = taskCurrent;
                mutexes[psp[0]].lock = true;
                tcb[taskCurrent].mutex = psp[0];
            }
            break;
    }
}



