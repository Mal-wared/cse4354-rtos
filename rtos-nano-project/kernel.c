#include "kernel.h"
#include <stdint.h>
#include "uart0.h"
#include "util.h"
#include "stackHelper.h"
#include "kernel.h"
#include "tm4c123gh6pm.h"

uint32_t pid = 10000;

// page 148
void MpuISR()
{
    putsUart0("MPU fault in process ");
    printPid();
    while(1);
}

// page 92 - memory model: table 2-4 memory map
// page 126 - updating an MPU region
void triggerMpuFault() {
    // Configure MPU

    // MPU disabled b/c it cannot be configured while active
    NVIC_MPU_CTRL_R &= ~(NVIC_MPU_CTRL_ENABLE | NVIC_MPU_CTRL_HFNMIENA);
    NVIC_MPU_NUMBER_R = NVIC_MPU_NUMBER_M;

    // Attempt to write to flash memory (read-only)
    volatile uint32_t *ptr = (volatile uint32_t *)0x00001000;
    *ptr = 0xDEADBEEF;

}

// page 149
void BusISR()
{
    putsUart0("Bus fault in process ");
    printPid();
    while(1);
}

void triggerBusFault() {
    *(volatile uint32_t *)0x30000000 = 0;
}

// page 150
void UsageISR()
{
    putsUart0("Usage fault in process ");
    printPid();
    while(1);
}

void triggerUsageFault() {
    volatile int x = 10;
    volatile int y = 0;
    volatile int z = x / y; // This will now trigger a usage fault.
}

// Page 112 - Fault Types: Table 2-11 Faults
void HardFaultISR()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Hard fault in process ");
    printPid();
    putsUart0("\n");

    uint32_t debugFlags =    PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS |
                            PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP;
    printFaultDebug(debugFlags);

    // display mfault flags (in hex)
    // display offending instruction
    // display process stack dump (xPSR, PC, LR R0-3, R12)
    while(1);
}

void triggerHardFault() {
    // Disables UsageFault handler to force fault escalation
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_USAGE;
    volatile int x = 10;
    volatile int y = 0;

    // Divide-by-zero trap generates a UsageFault. Since its handler is disabled,
    // it escalates to HardFault handler.
    volatile int z = x / y;
}

void PendSvIsr() {

}

void triggerPendSvFault() {

}

void printPid() {
    // String of size 12 for 10 digits (4,294,967,295) + 1 sign + 1 null terminator
    char pidStr[12];
    itoa(pid, pidStr);
    putsUart0(pidStr);
    putsUart0("\n");
}

void printFaultDebug(uint32_t flags) {
    // Print PSP and MSP addresses
    if (flags & PRINT_STACK_POINTERS) {
        uint32_t currentMsp = getMsp();
        char mspStr[12];
        itoh(currentMsp, mspStr);

        uint32_t currentPsp = getPsp();
        char pspStr[12];
        itoh(currentPsp, pspStr);

        putsUart0("--- STACK POINTERS ---\n");
        putsUart0("MSP    (Main Stack Pointer):\t");
        putsUart0(mspStr);
        putsUart0("\n");

        putsUart0("PSP (Process Stack Pointer):\t");
        putsUart0(pspStr);
        putsUart0("\n\n");
    }

    // Print mfault flags (in hex)
    if (flags & PRINT_MFAULT_FLAGS) {
        uint32_t currentMsp = getMsp();
        char mspStr[12];
        itoh(currentMsp, mspStr);

        uint32_t currentPsp = getPsp();
        char pspStr[12];
        itoh(currentPsp, pspStr);

        putsUart0("--- STACK POINTERS ---\n");
        putsUart0("MSP    (Main Stack Pointer):\t");
        putsUart0(mspStr);
        putsUart0("\n");

        putsUart0("PSP (Process Stack Pointer):\t");
        putsUart0(pspStr);
        putsUart0("\n\n");
    }

    if (flags & PRINT_OFFENDING_INSTRUCTION) {
        // print offending instruction
        putsUart0("printing offending instruction");
    }

    if (flags & PRINT_DATA_ADDRESSES) {
        // print data addresses
        putsUart0("printing data addresses");
    }

    if (flags & PRINT_STACK_DUMP) {
        // print stack dump
        putsUart0("printing stack dump");
    }
}
