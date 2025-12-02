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
#include "shell.h"
#include <stdbool.h>
#include <string.h>
#include "clock.h"
#include "uart0.h"
#include "kernel.h"
#include "util.h"
#include "stackHelper.h"
#include "gpio.h"
#include "tasks.h"
#include "faults.h"

//-----------------------------------------------------------------------------
// Shell Variables
//-----------------------------------------------------------------------------

extern bool preemption;
TaskInfo taskInfo[MAX_TASKS];
MutexInfo mutexInfo[MAX_MUTEXES];
SemaphoreInfo semaphoreInfo[MAX_SEMAPHORES];

// REQUIRED: Add header files here for your strings functions, ...

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: add processing for the shell commands through the UART here

void reboot(void)
{
    putsUart0("REBOOTING\n");
}

void ps(void)
{
    TaskInfo info;
    int i;
    int k;
    char buffer[16];

    putsUart0(
            "PID     Name         State              Remaining Ticks   Priority   CPU %\n");
    putsUart0(
            "---     -----------  ----------------   ---------------   --------   -----\n");

    for (i = 0; i < MAX_TASKS; i++)
    {
        if (populateTaskInfo(i, &info))
        {
            if (info.state != STATE_INVALID)
            {
                // Print PID
                itoa(info.pid, buffer);
                putsUart0(buffer);

                for (k = 0; k < (8 - strlen(buffer)); k++)
                    putsUart0(" ");

                // Print Name
                putsUart0(info.name);
                for (k = 0; k < (13 - strlen(info.name)); k++)
                    putsUart0(" ");

                switch (info.state)
                {
                case STATE_UNRUN:
                    putsUart0("1: ");
                    putsUart0("UNRUN           ");
                    break;
                case STATE_READY:
                    putsUart0("2: ");
                    putsUart0("READY           ");
                    break;
                case STATE_DELAYED:
                    putsUart0("3: ");
                    putsUart0("DELAYED         ");
                    break;
                case STATE_BLOCKED_SEMAPHORE:
                    putsUart0("4: ");
                    putsUart0("BLOCKED (Sem)   ");
                    break;
                case STATE_BLOCKED_MUTEX:
                    putsUart0("5: ");
                    putsUart0("BLOCKED (Mut)   ");
                    break;
                case STATE_KILLED:
                    putsUart0("6: ");
                    putsUart0("KILLED          ");
                    break;
                default:
                    putsUart0("UNKNOWN         ");
                    break;
                }

                // Print remaining ticks
                if (info.state == STATE_DELAYED)
                {
                    itoa(info.ticks, buffer);
                    putsUart0(buffer);

                    // Calculate padding (Header is roughly 15 chars wide + 3 spacing)
                    for (k = 0; k < (18 - strlen(buffer)); k++)
                        putsUart0(" ");
                }
                else
                {
                    putsUart0("                  "); // 0 + 17 spaces
                }

                // Print Priority
                putsUart0("");
                itoa(info.priority, buffer);
                putsUart0(buffer);
                for (k = 0; k < (11 - strlen(buffer)); k++)
                    putsUart0(" ");

                if (info.totalTime > 0)
                {
                    // calculate percentage of cpu time here
                    uint32_t permille = (info.time * 10000) / info.totalTime;
                    uint32_t integer = permille / 100;
                    uint32_t decimal = permille % 100;

                    itoa(integer, buffer);
                    putsUart0(buffer);
                    putsUart0(".");
                    if (decimal < 10)
                    {
                        putsUart0("0"); // Leading zero for decimal
                    }
                    itoa(decimal, buffer);
                    putsUart0(buffer);
                }
                else
                {
                    putsUart0("0.00");
                }
                putsUart0("\n");
            }
        }
    }
}

void ipcs(void)
{
    char buffer[12];
    int i;
    int k;

    // Mutexes
    putsUart0("Mutexes\n");
    putsUart0("-------------------------------------------------------\n");
    putsUart0("Ref   Lock Status   Owner   Queue Size   Queue\n");
    putsUart0("---   -----------   -----   ----------   --------------\n");

    MutexInfo mInfo;
    for (i = 0; i < MAX_MUTEXES; i++)
    {
        if (getResourceInfo(0, i, &mInfo))
        {
            // idx print
            itoa(i, buffer);
            putsUart0(buffer);
            for (k = 0; k < (6 - strlen(buffer)); k++)
                putsUart0(" ");

            // lock state print
            if (mInfo.lock)
            {
                putsUart0("Locked\t");
                for (k = 0; k < (5 - strlen(mInfo.lock)); k++)
                    putsUart0(" ");
            }

            else
            {
                putsUart0("Unlocked\t");
                for (k = 0; k < (3 - strlen(mInfo.lock)); k++)
                    putsUart0(" ");
            }

            // owner print
            itoa(mInfo.lockedBy, buffer);
            putsUart0(buffer);
            for (k = 0; k < (8 - strlen(buffer)); k++)
                putsUart0(" ");

            // qeuue size print
            itoa(mInfo.queueSize, buffer);
            putsUart0(buffer);
            for (k = 0; k < (13 - strlen(buffer)); k++)
                putsUart0(" ");

            for (k = 0; k < mInfo.queueSize; k++)
            {
                itoa(mInfo.processQueue[k], buffer);
                putsUart0(buffer);
                putsUart0(" ");
            }
        }
    }

    // Mutexes
    putsUart0("\n\nSemaphores\n");
    putsUart0("--------------------------------\n");
    putsUart0("Ref   Count   Queue Size   Queue\n");
    putsUart0("---   -----   ----------   -----\n");

    SemaphoreInfo sInfo;
    for (i = 0; i < MAX_SEMAPHORES; i++)
    {
        if (getResourceInfo(1, i, &sInfo))
        {
            // idx print
            itoa(i, buffer);
            putsUart0(buffer);
            for (k = 0; k < (6 - strlen(buffer)); k++)
                putsUart0(" ");

            // count print
            itoa(sInfo.count, buffer);
            putsUart0(buffer);
            for (k = 0; k < (8 - strlen(buffer)); k++)
                putsUart0(" ");

            // queue size print
            itoa(sInfo.queueSize, buffer);
            putsUart0(buffer);
            for (k = 0; k < (13 - strlen(buffer)); k++)
                putsUart0(" ");

            for (k = 0; k < sInfo.queueSize; k++)
            {
                itoa(sInfo.processQueue[k], buffer);
                putsUart0(buffer);
                putsUart0(" ");
            }

            putsUart0("\n");
        }
    }
}

void kill(uint32_t pid)
{
    // SVC #6, kills thread
    destroyThread(pid);

    char pidStr[12];
    itoa(pid, pidStr);
    putsUart0(pidStr);
    putsUart0(" killed (if valid)\n");
}

void pkill(const char name[])
{
    int32_t pid = getPid(name);

    if (pid != -1)
    {
        destroyThread(pid); // SVC #6
        putsUart0("Process killed: ");
        putsUart0(name);
        putsUart0("\n");
    }
    else
    {
        putsUart0("Process not found\n");
    }
}

void pi(bool on)
{
    setPriorityInheritance(on);
    if (on)
    {
        putsUart0("pi on\n");
    }
    else
    {
        putsUart0("pi off\n");
    }
}

void preempt(bool on)
{
    setPreemption(on);
    if (on)
    {

        putsUart0("preempt on\n");
    }
    else
    {
        putsUart0("preempt off\n");
    }

}

void sched(bool prio_on)
{
    setSched(prio_on);
    if (prio_on)
    {
        putsUart0("sched prio\n");
    }
    else
    {
        putsUart0("sched rr\n");
    }
}

void pidof(const char name[])
{
    int32_t pid = getPid(name);

    if (pid != -1)
    {
        char buffer[10];
        itoa(pid, buffer);
        putsUart0(buffer);
        putsUart0("\n");
    }
    else
    {
        putsUart0("Process not found\n");
    }
}

void run(const char name[])
{
    launchTask(name);
    putsUart0("Task launched (if found)\n");

}

void shell(void)
{
    USER_DATA data;
    uint8_t count = 0;
    uint8_t i = 0;
    char c;
    bool entered = false;

    putsUart0("\r\n> ");

    while (true)
    {
        while (!kbhitUart0())
        {
            sleep(10);
        }

        // Shell operates as a two-state machine via the "entered" flag
        // State 1 (!entered):  Buffers user input until 'Enter' key is pressed
        // State 2 (entered):   Parses and executes the buffered command
        //
        // It is crucial to use two separate 'if' statements rather than an 'if-else' to allow
        // instant transition from input buffering to command execution.
        if (!entered)
        {
            c = getcUart0();
            if ((c == 8 || c == 127) && count > 0)
            {
                count--;
            }

            if (c == 13)
            {
                data.buffer[count] = '\0';
                entered = true;
            }

            if (c >= 32 && c != 127)
            {
                data.buffer[count] = c;
                count++;
                if (count == MAX_CHARS)
                {
                    data.buffer[count] = '\0';
                    entered = true;
                }
            }
        }

        if (entered)
        {
            entered = false;
            count = 0;
            bool valid = false;

            parseFields(&data);

            if (isCommand(&data, "reboot", 0))
            {
                valid = true;
                reboot();
            }

            if (isCommand(&data, "ps", 0))
            {
                valid = true;
                ps();
            }

            if (isCommand(&data, "ipcs", 0))
            {
                valid = true;
                ipcs();
            }

            if (isCommand(&data, "kill", 1))
            {
                // get arg as string just in case
                char *str = getFieldString(&data, 1);
                bool isNum = true;
                int k = 0;

                // 2. Check if every character is a digit
                while (str[k] != '\0')
                {
                    if (str[k] < '0' || str[k] > '9')
                    {
                        isNum = false;
                        break;
                    }
                    k++;
                }

                if (isNum)
                {
                    int32_t pid = getFieldInteger(&data, 1);

                    if (pid == 0)
                    {
                        putsUart0("ERROR: Cannot kill Idle task (PID 0)\n");
                    }
                    else
                    {
                        valid = true;
                        kill(pid);
                    }
                }
                else
                {
                    putsUart0(
                            "ERROR: Invalid PID (not a number). Did you mean 'pkill'?\n");
                    valid = true;
                }
            }

            if (isCommand(&data, "pkill", 1))
            {
                char *proc_name = getFieldString(&data, 1);

                valid = true;
                pkill(proc_name);
            }

            if (isCommand(&data, "pi", 1))
            {
                bool on;
                char *piStr = getFieldString(&data, 1);
                if (stricmp(piStr, "ON") == 0)
                {
                    on = true;
                    valid = true;
                    pi(on);
                }
                else if (stricmp(piStr, "OFF") == 0)
                {
                    on = false;
                    valid = true;
                    pi(on);
                }
            }

            if (isCommand(&data, "preempt", 1))
            {
                bool on;
                char *preemptStr = getFieldString(&data, 1);
                if (stricmp(preemptStr, "ON") == 0)
                {
                    on = true;
                    valid = true;
                    preempt(on);
                }
                else if (stricmp(preemptStr, "OFF") == 0)
                {
                    on = false;
                    valid = true;
                    preempt(on);
                }
            }

            if (isCommand(&data, "sched", 1))
            {
                bool on;
                char *schedStr = getFieldString(&data, 1);
                if (stricmp(schedStr, "PRIO") == 0)
                {
                    on = true;
                    valid = true;
                    sched(on);
                }
                else if (stricmp(schedStr, "RR") == 0)
                {
                    on = false;
                    valid = true;
                    sched(on);
                }
            }

            if (isCommand(&data, "pidof", 1))
            {
                char *proc_name = getFieldString(&data, 1);
                valid = true;
                pidof(proc_name);
            }

            if (isCommand(&data, "run", 1))
            {
                char *proc_name = getFieldString(&data, 1);
                valid = true;
                run(proc_name);
            }

            if (isCommand(&data, "hard", 0))
            {
                valid = true;
                triggerHardFault();
            }

            if (isCommand(&data, "usage", 0))
            {
                valid = true;
                triggerUsageFault();
            }

            if (isCommand(&data, "bus", 0))
            {
                valid = true;
                triggerBusFault();
            }

            if (isCommand(&data, "mpu", 0))
            {
                valid = true;
                triggerMpuFault();
            }

            if (isCommand(&data, "pendsv", 0))
            {
                valid = true;
                triggerPendSvFault();
            }

            if (isCommand(&data, "demo", 1))
            {
                char *param = getFieldString(&data, 1);
                if (stricmp(param, "priv") == 0)
                {
                    valid = true;
                    testSRAMpriv();
                }
                if (stricmp(param, "unpriv") == 0)
                {
                    valid = true;
                    testSRAMunpriv();
                }
                if (stricmp(param, "free") == 0)
                {
                    valid = true;
                    testSRAMunprivFree();
                }

            }

            if (!valid)
            {
                putsUart0("Invalid command\n");
            }

            for (i = 0; i < MAX_CHARS + 1; i++)
            {
                data.buffer[i] = '\0';
            }

            for (i = 0; i < MAX_FIELDS; i++)
            {
                data.fieldPosition[i] = 0;
                data.fieldType[i] = 0;
            }
            data.fieldCount = 0;
            putcUart0('\n');

            // Prompt carat
            putsUart0("> ");
        }
    }
}
