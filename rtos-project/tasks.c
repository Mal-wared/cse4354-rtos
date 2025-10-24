// Tasks
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
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "wait.h"
#include "kernel.h"
#include "tasks.h"
#include "clock.h"
#include "nvic.h"
#include "uart0.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
// REQUIRED: Add initialization for blue, orange, red, green, and yellow LEDs
//           Add initialization for 6 pushbuttons
void initHw(void)
{
    // Setup LEDs and pushbuttons
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTA);
    enablePort(PORTB);
    enablePort(PORTE);
    enablePort(PORTF);

    // Configure LEDs
    selectPinPushPullOutput(BLUE_LED);
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(ORANGE_LED);
    selectPinPushPullOutput(YELLOW_LED);
    selectPinPushPullOutput(GREEN_LED);

    // Configure pushbuttons
    selectPinDigitalInput(BUTTON1);
    selectPinDigitalInput(BUTTON2);
    selectPinDigitalInput(BUTTON3);
    selectPinDigitalInput(BUTTON4);
    selectPinDigitalInput(BUTTON5);
    selectPinDigitalInput(BUTTON6);

    enablePinPulldown(BUTTON1);
    enablePinPulldown(BUTTON2);
    enablePinPulldown(BUTTON3);
    enablePinPulldown(BUTTON4);
    enablePinPulldown(BUTTON5);
    enablePinPulldown(BUTTON6);

    selectPinInterruptFallingEdge(BUTTON1);
    selectPinInterruptFallingEdge(BUTTON2);
    selectPinInterruptFallingEdge(BUTTON3);
    selectPinInterruptFallingEdge(BUTTON4);
    selectPinInterruptFallingEdge(BUTTON5);
    selectPinInterruptFallingEdge(BUTTON6);

    clearPinInterrupt(BUTTON1);
    clearPinInterrupt(BUTTON2);
    clearPinInterrupt(BUTTON3);
    clearPinInterrupt(BUTTON4);
    clearPinInterrupt(BUTTON5);
    clearPinInterrupt(BUTTON6);

    enableNvicInterrupt(INT_GPIOA);
    enableNvicInterrupt(INT_GPIOB);
    enableNvicInterrupt(INT_GPIOE);

    // Power-up flash
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(250000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(250000);

    // Enable Bus, Usage, and Memory Faults
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_USAGE
            | NVIC_SYS_HND_CTRL_MEM;

    // Enable divide-by-zero trap
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;
}

// REQUIRED: add code to return a value from 0-63 indicating which of 6 PBs are pressed
uint8_t readPbs(void)
{
    int8_t pb = 0;
    if (getPinValue(BUTTON1)) pb |= (1 << 0);
    if (getPinValue(BUTTON2)) pb |= (1 << 1);
    if (getPinValue(BUTTON3)) pb |= (1 << 2);
    if (getPinValue(BUTTON4)) pb |= (1 << 3);
    if (getPinValue(BUTTON5)) pb |= (1 << 4);
    if (getPinValue(BUTTON6)) pb |= (1 << 5);
    return pb;
}

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle(void)
{
    while(true)
    {
        setPinValue(ORANGE_LED, 1);
        waitMicrosecond(1000);
        setPinValue(ORANGE_LED, 0);
        yield();
    }
}

void flash4Hz(void)
{
    while(true)
    {
        setPinValue(GREEN_LED, !getPinValue(GREEN_LED));
        sleep(125);
    }
}

void oneshot(void)
{
    while(true)
    {
        wait(flashReq);
        setPinValue(YELLOW_LED, 1);
        sleep(1000);
        setPinValue(YELLOW_LED, 0);
    }
}

void partOfLengthyFn(void)
{
    // represent some lengthy operation
    waitMicrosecond(990);
    // give another process a chance to run
    yield();
}

void lengthyFn(void)
{
    uint16_t i;
    while(true)
    {
        lock(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
        }
        setPinValue(RED_LED, !getPinValue(RED_LED));
        unlock(resource);
    }
}

void readKeys(void)
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            putsUart0("push button 1");
            setPinValue(YELLOW_LED, !getPinValue(YELLOW_LED));
            setPinValue(RED_LED, 1);
        }
        if ((buttons & 2) != 0)
        {
            putsUart0("push button 2");
            post(flashReq);
            setPinValue(RED_LED, 0);
        }
        if ((buttons & 4) != 0)
        {
            putsUart0("push button 3");
            restartThread(flash4Hz);
        }
        if ((buttons & 8) != 0)
        {
            putsUart0("push button 4");
            killThread(flash4Hz);
        }
        if ((buttons & 16) != 0)
        {
            putsUart0("push button 5");
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce(void)
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--;
            else
                count = 10;
        }
        post(keyReleased);
    }
}

void uncooperative(void)
{
    while(true)
    {
        while (readPbs() == 8)
        {
        }
        yield();
    }
}

void errant(void)
{
    uint32_t* p = (uint32_t*)0x20000000;
    while(true)
    {
        while (readPbs() == 32)
        {
            *p = 0;
        }
        yield();
    }
}

void important(void)
{
    while(true)
    {
        lock(resource);
        setPinValue(BLUE_LED, 1);
        sleep(1000);
        setPinValue(BLUE_LED, 0);
        unlock(resource);
    }
}
