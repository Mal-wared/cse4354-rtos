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

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    // Initialize variables
    USER_DATA data;

	// Initialize hardware
	initHw();
	initUart0();

	while (true)
    {
        data.fieldCount = 0;
        bool valid = false;
        getsUart0(&data);
        putsUart0(data.buffer);
        putcUart0('\n');
        parseFields(&data);
        uint8_t i = 0;
        for (i = 0; i < data.fieldCount; i++)
        {
            putcUart0(data.fieldType[i]);
            putcUart0('\t');
            putsUart0(&(data.buffer[data.fieldPosition[i]]));
            putcUart0('\n');
        }

        if (isCommand(&data, "set", 2))
        {
            int32_t add1 = getFieldInteger(&data, 1);
            int32_t add2 = getFieldInteger(&data, 2);
            valid = true;

            // do something with this information
            if (add1 > add2)
            {
                putsUart0("add1 is bigger");
                putcUart0('\n');
            }
            else if (add1 < add2)
            {
                putsUart0("add2 is bigger");
                putcUart0('\n');
            }
            else
            {
                putsUart0("add1 and and2 are same");
                putcUart0('\n');
            }
        }

        if (isCommand(&data, "alert", 1))
        {
            char* alertStr = getFieldString(&data, 1);
            valid = true;
            putsUart0("ALERT ");
            putsUart0(alertStr);
            putcUart0('\n');
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

    }

    return 0;
}
