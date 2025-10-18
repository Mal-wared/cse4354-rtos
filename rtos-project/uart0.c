// UART0 Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "util.h"

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0()
{
    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;
    _delay_cycles(3);

    // Configure UART0 pins
    GPIO_PORTA_DR2R_R |= UART_TX_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTA_DEN_R |= UART_TX_MASK | UART_RX_MASK; // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R |= UART_TX_MASK | UART_RX_MASK; // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M); // clear bits 0-7
    GPIO_PORTA_PCTL_R |= GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
    // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud, 8N1 format
    UART0_CTL_R = 0;                 // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                 // use system clock (40 MHz)
    UART0_IBRD_R = 21; // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN; // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
    // enable TX, RX, and module
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate; // calculate divisor (r) in units of 1/128,
                                                      // where r = fcyc / 16 * baudRate
    divisorTimes128 += 1;                         // add 1/128 to allow rounding
    UART0_CTL_R = 0;                 // turn-off UART0 to allow safe programming
    UART0_IBRD_R = divisorTimes128 >> 7;        // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128) >> 1) & 63; // set fractional value to round(fract(r)*64)
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN; // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
    // turn-on UART0
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF)
        ;               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char *str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
    while (UART0_FR_R & UART_FR_RXFE)
        ;               // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}

// Function to receive characters from the user interface, processing special characters such as backspace and writing the resultant into the buffer
void getsUart0(USER_DATA *data)
{
    uint8_t count = 0;
    while (true)
    {
        char c = getcUart0();
        if ((c == 8 || c == 127) && count > 0)
        {
            count--;
        }

        if (c == 13)
        {
            // add a null terminator to end of the string
            data->buffer[count] = '\0';
            return;
        }

        if (c >= 32 && c != 127)
        {
            data->buffer[count] = c;
            count++;
            if (count == MAX_CHARS)
            {
                data->buffer[count] = '\0';
                return;
            }
        }
    }
}

//
void parseFields(USER_DATA *data)
{
    uint8_t count = 0;
    uint8_t position = 0;
    char prevType = 'd';
    while (data->buffer[count] != '\0')
    {
        char c = data->buffer[count];
        if (prevType == 'd')
        {
            if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122))
            {
                prevType = 'a';
            }
            else if (c >= 48 && c <= 57)
            {
                prevType = 'n';
            }
            data->fieldType[position] = prevType;
            data->fieldPosition[position] = count++;
            position++;
            data->fieldCount++;
        }
        else
        {
            if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122))
            {
                prevType = 'a';
            }
            else if (c >= 48 && c <= 57)
            {
                prevType = 'n';
            }
            else
            {
                prevType = 'd';
                data->buffer[count] = '\0';
            }
            count++;
        }

    }
    return;
}

// Gets the integer value of the desired field number
// ** Assumes the entire field is an integer **
int32_t getFieldInteger(USER_DATA *data, uint8_t fieldNumber)
{
    if (fieldNumber < data->fieldCount && data->fieldType[fieldNumber] == 'n')
    {
        return atoi(&(data->buffer[data->fieldPosition[fieldNumber]]));
    }
    return 0;
}

//
char* getFieldString(USER_DATA *data, uint8_t fieldNumber)
{
    if (fieldNumber < data->fieldCount)
    {
        char *fieldString = &(data->buffer[data->fieldPosition[fieldNumber]]);
        return fieldString;
    }
    return (void*) 0;
}

bool isCommand(USER_DATA *data, const char strCommand[], uint8_t minArguments)
{
    int count = 0;
    char *fieldStr = getFieldString(data, 0);

    // Check is a correct # of arguments
    int numOfArguments = data->fieldCount - 1;
    if (numOfArguments < minArguments)
    {
        return false;
    }

    // Check if command is equal
    while (strCommand[count] != '\0' && fieldStr[count] != '\0')
    {
        if (tolower(strCommand[count]) != tolower(fieldStr[count]))
        {
            return false;
        }
        count++;
    }

    if (strCommand[count] != '\0' || fieldStr[count] != '\0')
    {
        return false;
    }
    return true;
}

// Returns the status of the receive buffer
bool kbhitUart0()
{
    return !(UART0_FR_R & UART_FR_RXFE);
}
