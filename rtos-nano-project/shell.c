// Nano Project
// Nicholas Nhat Tran 1002027150

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Red LED:
//   PF1 drives an NPN transistor that powers the red LED
// Green LED:
//   PF3 drives an NPN transistor that powers the green LED
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "clock.h"
#include "uart0.h"
#include "atoi.h"
#include "tm4c123gh6pm.h"

// Bitband aliases
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4)))
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))

// PortF masks
#define GREEN_LED_MASK 8
#define RED_LED_MASK 2

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCGPIO_R = SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    // Configure LED pins
    GPIO_PORTF_DIR_R |= GREEN_LED_MASK | RED_LED_MASK;  // bits 1 and 3 are outputs
    GPIO_PORTF_DR2R_R |= GREEN_LED_MASK | RED_LED_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTF_DEN_R |= GREEN_LED_MASK | RED_LED_MASK;  // enable LEDs
}

void yield(void)
{

}

void reboot(void)
{
    putsUart0("REBOOTING\n");
}

void ps(void)
{
    putsUart0("PS called\n");
}

void ipcs(void)
{
    putsUart0("IPCS called\n");
}

void kill(uint32_t pid)
{
    char pidStr[MAX_CHARS];
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
    RED_LED ^= 1;
    if (RED_LED == 0)
    {
        putsUart0("RED_LED turned off");
    }
    else
    {
        putsUart0("RED_LED turned on");
    }

}

void shell(void)
{
    // Initialize variables
    USER_DATA data;
    uint8_t count = 0;
    uint8_t i = 0;
    char c;
    bool entered = false;

    // Initialize hardware
    initHw();
    initUart0();

    while (true)
    {
        while (!kbhitUart0())
        {
            yield();
        }

        if (!entered)
        {
            c = getcUart0();
            if ((c == 8 || c == 127) && count > 0) {
                count--;
            }

            if (c == 13) {
                // add a null terminator to end of the string
                data.buffer[count] = '\0';
                entered = true;
            }

            if (c >= 32 && c != 127) {
                data.buffer[count] = c;
                count++;
                if (count == MAX_CHARS) {
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

            // putsUart0(data.buffer); // DEBUGGING: prints command to screen
            parseFields(&data);

            /* DEBUGGING
            for (i = 0; i < data.fieldCount; i++)
            {
                putcUart0(data.fieldType[i]);
                putcUart0('\t');
                putsUart0(&(data.buffer[data.fieldPosition[i]]));
                putcUart0('\n');
            }
            */

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
                char* proc_name = getFieldString(&data, 1);

                valid = true;
                pkill(proc_name);
            }

            if (isCommand(&data, "pi", 1))
            {
                bool on;
                char* piStr = getFieldString(&data, 1);
                if (strcmp(piStr, "ON") == 0)
                {
                    on = true;
                    valid = true;
                    pi(on);
                }
                else if (strcmp(piStr, "OFF") == 0)
                {
                    on = false;
                    valid = true;
                    pi(on);
                }
            }

            if (isCommand(&data, "preempt", 1))
            {
                bool on;
                char* preemptStr = getFieldString(&data, 1);
                if (strcmp(preemptStr, "ON") == 0)
                {
                    on = true;
                    valid = true;
                    preempt(on);
                }
                else if (strcmp(preemptStr, "OFF") == 0)
                {
                    on = false;
                    valid = true;
                    preempt(on);
                }
            }

            if (isCommand(&data, "sched", 1))
            {
                bool on;
                char* schedStr = getFieldString(&data, 1);
                if (strcmp(schedStr, "PRIO") == 0)
                {
                    on = true;
                    valid = true;
                    sched(on);
                }
                else if (strcmp(schedStr, "RR") == 0)
                {
                    on = false;
                    valid = true;
                    sched(on);
                }
            }

            if (isCommand(&data, "pidof", 1))
            {
                char* proc_name = getFieldString(&data, 1);
                valid = true;
                pidof(proc_name);
            }

            if (isCommand(&data, "run", 1))
            {

                char* proc_name = getFieldString(&data, 1);
                valid = true;
                run(proc_name);
            }

            if (!valid) {
                putsUart0("Invalid command\n");
            }

            for (i = 0; i < MAX_CHARS + 1; i++) {
                data.buffer[i] = '\0';
            }

            for (i = 0; i < MAX_FIELDS; i++)
            {
                data.fieldPosition[i] = 0;
                data.fieldType[i] = 0;
            }
            data.fieldCount = 0;
            putcUart0('\n');
        }
    }
}
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    shell();

    return 0;
}
