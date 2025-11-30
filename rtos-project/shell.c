// Shell functions
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
    rtosPs();
}

void ipcs(void)
{
    putsUart0("IPCS called\n");
}

void kill(uint32_t pid)
{
    char pidStr[12];
    itoa(pid, pidStr);
    putsUart0(pidStr);
    putsUart0(" killed\n");
}

void pkill(const char name[])
{
    putsUart0(name);
    putsUart0(" killed\n");
}

void pi(bool on)
{
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
    putsUart0(name);
    putsUart0(" launched\n");
}

void run(const char name[])
{
    if (getPinValue(BLUE_LED))
    {
        setPinValue(BLUE_LED, 0);
        putsUart0("RED_LED turned off");
    }
    else
    {
        setPinValue(BLUE_LED, 1);
        putsUart0("RED_LED turned on");
    }

}

void shell(void)
{
    USER_DATA data;
    uint8_t count = 0;
    uint8_t i = 0;
    char c;
    bool entered = false;

    putsUart0("\n> ");

    while (true)
    {
        while (!kbhitUart0())
        {
            yield();
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
                int32_t pid = getFieldInteger(&data, 1);
                valid = true;
                kill(pid);
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
