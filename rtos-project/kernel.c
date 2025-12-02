// Nicholas Nhat Tran
// 1002027150

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
mutex mutexes[MAX_MUTEXES];

// semaphore
semaphore semaphores[MAX_SEMAPHORES];

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks

// control
bool priorityScheduler = true;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = true;          // preemption (true) or cooperative (false)

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
    void *stackBase;
    uint32_t time;
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
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_ENABLE | NVIC_ST_CTRL_INTEN
            | NVIC_ST_CTRL_CLK_SRC;

    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
        tcb[i].time = 0;
    }
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void)
{
    bool ok;
    static uint8_t task = 0xFF;
    ok = false;
    if (priorityScheduler)
    {
        uint8_t highestPrio = 255;
        uint8_t start = (taskCurrent + 1) % MAX_TASKS;
        int i = start;
        task = 0;
        do
        {
            if (tcb[i].state == STATE_READY || tcb[i].state == STATE_UNRUN)
            {
                if (tcb[i].currentPriority < highestPrio) // Check dynamic priority
                {
                    task = i;
                    highestPrio = tcb[task].currentPriority;
                }
            }
            i = (i + 1) % MAX_TASKS; // wrap around to not pick current task for next task
        }
        while (i != start);
        ok = true;
    }
    else
    {
        while (!ok)
        {
            task++;
            if (task >= MAX_TASKS)
                task = 0;
            ok = (tcb[task].state == STATE_READY
                    || tcb[task].state == STATE_UNRUN);
        }
    }

    if (task == 0xFF)
    {
        task = 0;
    }

    if (tcb[task].state == STATE_UNRUN)
    {
        tcb[task].state = STATE_READY;
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

    uint32_t *psp = tcb[taskCurrent].sp;
    setPsp(psp);
    loadR3((uint32_t) tcb[taskCurrent].pid);
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
                if (i >= MAX_TASKS)
                {
                    // No available space in the tcb
                    return false;
                }
            }

            // allocate memory for the process
            void *stack = mallocHeap(stackBytes);
            if (stack == NULL)
            {
                return false;
            }
            tcb[i].stackBase = stack;

            // set srd bits for tcb
            tcb[i].srd = createNoSramAccessMask();
            addSramAccessWindow(&tcb[i].srd, (uint32_t*) stack, stackBytes);

            // set stack pointer dummy variables
            uint32_t *sp = (uint32_t*) ((uint32_t) stack + stackBytes);
            *(--sp) = 0x01000000;     // xPSR
            *(--sp) = (uint32_t) fn;  // PC
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
    __asm(" SVC #6 ");
}

void destroyThread(uint32_t pid)
{
    __asm(" SVC #6 ");
}

// REQUIRED: modify this function to restart a thread, including creating a stack
void restartThread(_fn fn)
{
    __asm(" SVC #11 ");
}
void restartThreadKernel(_fn fn)
{
    int i;
    int taskIndex = -1;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (tcb[i].pid == fn)
        {
            taskIndex = i;
            break;
        }
    }

    if (taskIndex != -1
            && (tcb[taskIndex].state == STATE_KILLED
                    || tcb[taskIndex].state == STATE_UNRUN))
    {
        uint32_t stackBytes = 1024;
        void *stack = mallocHeap(stackBytes);
        if (stack == NULL)
        {
            return;
        }

        tcb[taskIndex].stackBase = stack;

        tcb[taskIndex].srd = createNoSramAccessMask();
        addSramAccessWindow(&tcb[taskIndex].srd, (uint32_t*) stack, stackBytes);

        uint32_t *sp = (uint32_t*) ((uint32_t) stack + stackBytes);
        *(--sp) = 0x01000000;     // xPSR
        *(--sp) = (uint32_t) fn;  // PC
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

        tcb[taskIndex].sp = sp;

        // Reset State
        tcb[taskIndex].state = STATE_READY;

    }
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
    __asm(" SVC #14 ");
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
    __asm(" SVC #4 ");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm(" SVC #5 ");
}

// REQUIRED: modify this function to lock a mutex using pendsv
void lock(int8_t mutex)
{
    __asm(" SVC #2 ");
}

// REQUIRED: modify this function to unlock a mutex using pendsv
void unlock(int8_t mutex)
{
    __asm(" SVC #3 ");
}

void testSRAMpriv()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = { 512, 1024, 512, 1536, 1024, 1024, 1024,
                                    1024, 512, 4096 };
    int i;
    for (i = 0; i < 10; i++)
    {
        pointers[i] = mallocHeap(malloc_regions[i]);
    }
    applySramAccessMask(mask);
    *pointers[2] = 10;

    for (i = 0; i < 10; i++)
    {
        freeHeap(pointers[i]);
    }

}

// can't free heap in unprivileged mode
void testSRAMunpriv()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = { 512, 1024, 512, 1536, 1024, 1024, 1024,
                                    1024, 512, 4096 };
    int i;
    for (i = 0; i < 10; i++)
    {
        pointers[i] = mallocHeap(malloc_regions[i]);
    }
    applySramAccessMask(mask);
    setTMPL();
    *pointers[2] = 10;
}

void testSRAMunprivFree()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = { 512, 1024, 512, 1536, 1024, 1024, 1024,
                                    1024, 512, 4096 };
    int i;
    for (i = 0; i < 10; i++)
    {
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
    tcb[taskCurrent].time++;

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

    if (preemption)
    {
        triggerPendSvFault();
    }
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr(void)
{
    tcb[taskCurrent].sp = saveContext();

//    putsUart0("--- PENDSV HANDLER ---\n");
//    putsUart0("Pendsv in process ");
//    printPid(0);

    if (NVIC_FAULT_STAT_R & (NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR))
    {
        NVIC_FAULT_STAT_R &= ~(NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR);
//        putsUart0(", called from MPU\n\n");
    }
    else
    {
//        putsUart0("\n\n");
    }

//    uint32_t debugFlags = PRINT_MFAULT_FLAGS;
//    printFaultDebug(debugFlags);

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
    uint32_t *psp = getPsp();
    uint32_t *stacked_pc = (uint32_t*) psp[6];
    uint8_t *pc = (uint8_t*) stacked_pc;
    pc = pc - 2;
    uint8_t svcCallNum = *pc;

    switch (svcCallNum)
    {
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
            mutexes[psp[0]].processQueue[mutexes[psp[0]].queueSize] =
                    taskCurrent;
            mutexes[psp[0]].queueSize++;
            tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;
            tcb[taskCurrent].mutex = psp[0];

            if (priorityInheritance)
            {
                uint8_t owner = mutexes[psp[0]].lockedBy;
                // If the blocked task (current) is more important than the owner
                if (tcb[taskCurrent].currentPriority
                        < tcb[owner].currentPriority)
                {
                    // Promote the owner to the higher priority
                    tcb[owner].currentPriority =
                            tcb[taskCurrent].currentPriority;
                }
            }

            triggerPendSvFault();
        }
        else
        {
            mutexes[psp[0]].lockedBy = taskCurrent;
            mutexes[psp[0]].lock = true;
            tcb[taskCurrent].mutex = psp[0];
        }
        break;
    case 3:
        if (mutexes[psp[0]].lockedBy == taskCurrent) // Only owner can unlock
        {
            if (priorityInheritance)
            {
                // Restore the task to its original base priority
                tcb[taskCurrent].currentPriority = tcb[taskCurrent].priority;
            }

            if (mutexes[psp[0]].queueSize > 0)
            {
                uint8_t newMutexOwner = mutexes[psp[0]].processQueue[0];
                tcb[newMutexOwner].state = STATE_READY;
                mutexes[psp[0]].lockedBy = newMutexOwner;

                int i = 0;
                for (i = 0; i < mutexes[psp[0]].queueSize - 1; i++)
                {
                    mutexes[psp[0]].processQueue[i] =
                            mutexes[psp[0]].processQueue[i + 1];
                }

                mutexes[psp[0]].queueSize--;
            }
            else
            {
                // No one waiting: just unlock
                mutexes[psp[0]].lock = false;
                mutexes[psp[0]].lockedBy = 0;
            }
            // Update the current task's record to show it holds nothing
            tcb[taskCurrent].mutex = 0;
        }
        break;
    case 4:
        if (semaphores[psp[0]].count == 0)
        {
            semaphores[psp[0]].processQueue[semaphores[psp[0]].queueSize] =
                    taskCurrent;
            semaphores[psp[0]].queueSize++;
            tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;
            tcb[taskCurrent].semaphore = psp[0];
            triggerPendSvFault();
        }
        else
        {
            semaphores[psp[0]].count--;
        }
        break;
    case 5:
        if (semaphores[psp[0]].queueSize > 0)
        {
            uint8_t waitingTask = semaphores[psp[0]].processQueue[0];
            tcb[waitingTask].state = STATE_READY;
            if (tcb[waitingTask].priority < tcb[taskCurrent].priority)
            {
                triggerPendSvFault();
            }

            int i = 0;
            for (i = 0; i < semaphores[psp[0]].queueSize - 1; i++)
            {
                semaphores[psp[0]].processQueue[i] =
                        semaphores[psp[0]].processQueue[i + 1];
            }
            semaphores[psp[0]].queueSize--;
        }
        else
        {
            semaphores[psp[0]].count++;
        }
        break;
    case 6:
    {
        uint32_t input = psp[0];
        int taskToKill = -1;

        // if num is small, it's an index
        if (input < MAX_TASKS)
        {
            taskToKill = input;
        }
        else
        {
            int i;
            for (i = 0; i < MAX_TASKS; i++)
            {
                if (tcb[i].pid == (void*) input)
                {
                    taskToKill = i;
                    break;
                }
            }
        }

        forceKillThread(taskToKill);
        if (taskToKill == taskCurrent)
        {
            triggerPendSvFault();
        }
    }
        break;

    case 7:
    {
        // arg1: task idx
        // arg2: taskInfo struct
        uint8_t index = (uint8_t) psp[0];
        TaskInfo *info = (TaskInfo*) psp[1];

        if (index >= MAX_TASKS || tcb[index].state == STATE_INVALID)
        {
            psp[0] = 0; // Return false (modify R0 on stack)
        }
        else
        {
            // Copy data from internal TCB to the Shell's provided pointer
            info->pid = (uint32_t) tcb[index].pid;
            strncpy(info->name, tcb[index].name, 16);
            info->state = tcb[index].state;
            info->priority = tcb[index].priority;
            info->currentPriority = tcb[index].currentPriority;
            info->time = tcb[index].time;
            info->ticks = tcb[index].ticks;

            // Calculate total time (optional helper logic)
            uint32_t total = 0;
            int k;
            for (k = 0; k < MAX_TASKS; k++)
                total += tcb[k].time;
            info->totalTime = total;

            psp[0] = 1; // Return true
        }
    }
        break;
    case 8:
    {
        // Type 0: Mutex
        // Type 1: Semaphore
        int i;
        uint8_t type = (uint8_t) psp[0];
        uint8_t index = (uint8_t) psp[1];

        if (type == 0) // Mutex
        {
            MutexInfo *info = (MutexInfo*) psp[2];
            if (index < MAX_MUTEXES)
            {
                info->lock = mutexes[index].lock;
                info->lockedBy = mutexes[index].lockedBy;
                info->queueSize = mutexes[index].queueSize;
                for (i = 0; i < info->queueSize; i++)
                {
                    info->processQueue[i] = mutexes[index].processQueue[i];
                }
                psp[0] = 1; // Success
            }
            else
            {
                psp[0] = 0; // Fail
            }
        }
        else // Semaphore
        {
            SemaphoreInfo *info = (SemaphoreInfo*) psp[2];
            if (index < MAX_SEMAPHORES)
            {
                info->count = semaphores[index].count;
                info->queueSize = semaphores[index].queueSize;
                for (i = 0; i < info->queueSize; i++)
                {
                    info->processQueue[i] = semaphores[index].processQueue[i];
                }
                psp[0] = 1; // Success
            }
            else
            {
                psp[0] = 0; // Fail
            }
        }
    }
        break;
    case 9:
    {
        // arg1: name of process
        char *nameToFind = (char*) psp[0];
        int32_t foundPid = -1;
        int i;

        for (i = 0; i < MAX_TASKS; i++)
        {
            // Check for valid task and matching string
            if (tcb[i].state != STATE_INVALID
                    && strcmp(tcb[i].name, nameToFind) == 0)
            {
                foundPid = i;
                break;
            }
        }

        // Write the result (either PID or -1) back to R0 on the stack
        psp[0] = (uint32_t) foundPid;
    }
        break;
    case 10:
    {
        char *name = (char*) psp[0];
        int i;
        // Find task by name
        for (i = 0; i < MAX_TASKS; i++)
        {
            if (strcmp(tcb[i].name, name) == 0)
            {
                restartThreadKernel((_fn) tcb[i].pid); // Call the internal helper
                break;
            }
        }
    }
        break;
    case 11:
        restartThreadKernel((_fn) psp[0]);
        break;
    case 12:
        preemption = (bool) psp[0];
        break;
    case 13:
        priorityInheritance = (bool) psp[0];
        break;
    case 14:
    {
        _fn fn = (_fn) psp[0];
        uint8_t prio = (uint8_t) psp[1];

        int i;
        for (i = 0; i < MAX_TASKS; i++)
        {
            // find matching function pointer
            if (tcb[i].pid == fn && tcb[i].state != STATE_INVALID)
            {
                tcb[i].priority = prio;

                // if PI isn't boosting task, update current prio
                // lowers prio
                if (tcb[i].currentPriority > prio)
                {
                    tcb[i].currentPriority = prio;
                }
                break;
            }
        }
    }
        break;
    case 15:
        priorityScheduler = (bool) psp[0];
        break;
    }

}

bool populateTaskInfo(uint8_t index, TaskInfo *info)
{
    __asm(" SVC #7 ");
}

// Type 0: Mutex
// Type 1: Semaphore
bool getResourceInfo(uint8_t type, uint8_t index, void *info)
{
    __asm(" SVC #8 ");
}

int32_t getPid(const char name[])
{
    __asm(" SVC #9 ");
}

void launchTask(const char name[])
{
    __asm(" SVC #10 ");
}

void setPreemption(bool on)
{
    __asm(" SVC #12 ");
}

void setPriorityInheritance(bool on)
{
    __asm(" SVC #13 ");
}

void setSched(bool prio_on)
{
    __asm(" SVC #15 ");
}

uint8_t getTaskCurrent()
{
    return taskCurrent;
}

void forceKillThread(int taskIndex)
{
    if (taskIndex
            < 0|| taskIndex >= MAX_TASKS || tcb[taskIndex].state == STATE_INVALID)
    {
        return;
    }
    // release mutexes held by task
    int m;
    for (m = 0; m < MAX_MUTEXES; m++)
    {
        if (mutexes[m].lockedBy == taskIndex)
        {
            mutexes[m].lockedBy = 0;
            mutexes[m].lock = false;

            // handle passing mutex to next in queue
            if (mutexes[m].queueSize > 0)
            {
                uint8_t nextTask = mutexes[m].processQueue[0];
                mutexes[m].lockedBy = nextTask;
                mutexes[m].lock = true;
                tcb[nextTask].state = STATE_READY;

                // Shift queue
                int q;
                for (q = 0; q < mutexes[m].queueSize - 1; q++)
                {
                    mutexes[m].processQueue[q] = mutexes[m].processQueue[q + 1];
                }
                mutexes[m].queueSize--;
            }
        }

        // Remove task from any Mutex waiting queues
        int q;
        for (q = 0; q < mutexes[m].queueSize; q++)
        {
            if (mutexes[m].processQueue[q] == taskIndex)
            {
                // Shift remaining items left to overwrite the killed task
                int k;
                for (k = q; k < mutexes[m].queueSize - 1; k++)
                {
                    mutexes[m].processQueue[k] = mutexes[m].processQueue[k + 1];
                }
                mutexes[m].queueSize--;
                q--; // Decrement q to check the new item at this index
            }
        }
    }

    // post semaphores held by task
    int s;
    for (s = 0; s < MAX_SEMAPHORES; s++)
    {
        int q;
        for (q = 0; q < semaphores[s].queueSize; q++)
        {
            if (semaphores[s].processQueue[q] == taskIndex)
            {
                // Shift remaining items left
                int k;
                for (k = q; k < semaphores[s].queueSize - 1; k++)
                {
                    semaphores[s].processQueue[k] = semaphores[s].processQueue[k
                            + 1];
                }
                semaphores[s].queueSize--;
                q--;
            }
        }
    }

    // free memory
    if (tcb[taskIndex].stackBase != NULL)
    {
        freeHeap(tcb[taskIndex].stackBase);
    }

    // mark as an invalid state, it has been effectively killed
    tcb[taskIndex].state = STATE_KILLED;

}
