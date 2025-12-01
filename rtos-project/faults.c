// Nicholas Nhat Tran
// 1002027150

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
#include "faults.h"
#include "uart0.h"
#include "mm.h"
#include "kernel.h"
#include "stackHelper.h"
#include "util.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: If these were written in assembly
//           omit this file and add a faults.s file

// REQUIRED: code this function
void mpuFaultIsr(void)
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("MPU fault in process ");
    printPid(1);
    putsUart0("\n");

    uint32_t debugFlags = PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS
            | PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP
            | PRINT_DATA_ADDRESSES;
    printFaultDebug(debugFlags);

    //NVIC_FAULT_STAT_R = (NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR);
    triggerPendSvFault();
}

// page 92 - memory model: table 2-4 memory map
// page 126 - updating an MPU region
void triggerMpuFault()
{
    uint32_t *ptr = mallocHeap(512);
    freeHeap(ptr);
    applySramAccessMask(createNoSramAccessMask());
    setTMPL();
    *ptr = 10;
}

// REQUIRED: code this function
void hardFaultIsr(void)
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Hard fault in process ");
    printPid(1);
    putsUart0("\n");

    uint32_t debugFlags = PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS
            | PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP;
    printFaultDebug(debugFlags);
    while (1)
    {
    }
}

void triggerHardFault()
{
    // Disables UsageFault handler to force fault escalation
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_USAGE;
    volatile int x = 10;
    volatile int y = 0;

    // Divide-by-zero trap generates a UsageFault. Since its handler is disabled,
    // it escalates to HardFault handler.
    volatile int z = x / y;
}

// REQUIRED: code this function
void busFaultIsr(void)
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Bus fault in process ");
    printPid(1);
    putsUart0("\n");

    uint32_t debugFlags = PRINT_MFAULT_FLAGS;
    printFaultDebug(debugFlags);

    while (1)
    {
    }
}

void triggerBusFault()
{
    // tries to access a reserved memory region, resulting in
    *(volatile uint32_t*) 0x30000000 = 0;
}

// REQUIRED: code this function
void usageFaultIsr(void)
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Usage fault in process ");
    printPid(1);
    putsUart0("\n");

    uint32_t debugFlags = PRINT_MFAULT_FLAGS;
    printFaultDebug(debugFlags);

    while (1)
    {
    }
}

void triggerUsageFault()
{
    volatile int x = 10;
    volatile int y = 0;
    volatile int z = x / y; // This will now trigger a usage fault.
}

void printFaultDebug(uint32_t flags)
{
    uint32_t *currentPsp = getPsp();
    uint32_t *currentMsp = getMsp();

    // Print PSP and MSP addresses
    if (flags & PRINT_STACK_POINTERS)
    {
        char mspStr[12];
        itoh((uint32_t) currentMsp, mspStr);

        char pspStr[12];
        itoh((uint32_t) currentPsp, pspStr);

        putsUart0("--- STACK POINTERS ---\n");
        putsUart0("MSP    (Main Stack Pointer):\t");
        putsUart0(mspStr);
        putsUart0("\n");

        putsUart0("PSP (Process Stack Pointer):\t");
        putsUart0(pspStr);
        putsUart0("\n\n");
    }

    if (flags & PRINT_STACK_DUMP)
    {
        char buffer[12];
        putsUart0("--- PROCESS STACK DUMP ---\n");

        putsUart0("R0:\t");
        itoh(currentPsp[0], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("R1:\t");
        itoh(currentPsp[1], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("R2:\t");
        itoh(currentPsp[2], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("R3:\t");
        itoh(currentPsp[3], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("R12:\t");
        itoh(currentPsp[4], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("LR:\t");
        itoh(currentPsp[5], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("PC:\t");
        itoh(currentPsp[6], buffer);
        putsUart0(buffer);
        putsUart0("\n");

        putsUart0("xPSR:\t");
        itoh(currentPsp[7], buffer);
        putsUart0(buffer);
        putsUart0("\n\n");
    }

    // Print mfault flags (in hex)
    if (flags & PRINT_MFAULT_FLAGS)
    {
        uint32_t faultStat = NVIC_FAULT_STAT_R;
        putsUart0("--- FAULT STATUS REGISTERS ---\n");
        putsUart0("Indicated causes of fault(s) in FAULTSTAT register:\n");
        if (faultStat & NVIC_FAULT_STAT_DIV0)
        {
            putsUart0("- [BIT 25] Divide-by-Zero Usage Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_UNALIGN)
        {
            putsUart0("- [BIT 24] Unaligned Access Usage Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_NOCP)
        {
            putsUart0("- [BIT 19] No Coprocessor Usage Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_INVPC)
        {
            putsUart0("- [BIT 18] Invalid PC Load Usage Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_INVSTAT)
        {
            putsUart0("- [BIT 17] Invalid State Usage Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_UNDEF)
        {
            putsUart0("- [BIT 16] Undefined Instruction Usage Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_BFARV)
        {
            putsUart0("- [BIT 15] Bus Fault Address Register Valid\n");
        }
        if (faultStat & NVIC_FAULT_STAT_BLSPERR)
        {
            putsUart0(
                    "- [BIT 13] Bus Fault on Floating-Point Lazy State Preservation\n");
        }
        if (faultStat & NVIC_FAULT_STAT_BSTKE)
        {
            putsUart0("- [BIT 12] Stack Bus Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_BUSTKE)
        {
            putsUart0("- [BIT 11] Unstack Bus Fault\n");
        }
        if (faultStat & NVIC_FAULT_STAT_IMPRE)
        {
            putsUart0("- [BIT 10] Imprecise Data Bus Error\n");
        }
        if (faultStat & NVIC_FAULT_STAT_PRECISE)
        {
            putsUart0("- [BIT 9] Precise Data Bus Error\n");
        }
        if (faultStat & NVIC_FAULT_STAT_IBUS)
        {
            putsUart0("- [BIT 8] Instruction Bus Error\n");
        }
        if (faultStat & NVIC_FAULT_STAT_MMARV)
        {
            putsUart0(
                    "- [BIT 7] Memory Management Fault Address Register Valid\n");
        }
        if (faultStat & NVIC_FAULT_STAT_MLSPERR)
        {
            putsUart0(
                    "- [BIT 5] Memory Management Fault on Floating-Point Lazy State Preservation\n");
        }
        if (faultStat & NVIC_FAULT_STAT_MSTKE)
        {
            putsUart0(
                    "- [BIT 4] Stack Access Violation (Memory Management Fault)\n");
        }
        if (faultStat & NVIC_FAULT_STAT_MUSTKE)
        {
            putsUart0(
                    "- [BIT 3] Unstack Access Violation (Memory Management Fault)\n");
        }
        if (faultStat & NVIC_FAULT_STAT_DERR)
        {
            putsUart0("- [BIT 1] Data Access Violation\n");
        }
        if (faultStat & NVIC_FAULT_STAT_IERR)
        {
            putsUart0("- [BIT 0] Instruction Access Violation\n");
        }
        putsUart0("\n");

    }

    // page 75 figure 2-3 cortex-m4f register set
    if (flags & PRINT_OFFENDING_INSTRUCTION)
    {
        // print offending instruction
        putsUart0("--- OFFENDING INSTRUCTION ---\n");
        uint32_t *faultingPc = (uint32_t*) currentPsp[6];
        uint32_t rawInstruction = *faultingPc;
        uint32_t ntohsInstruction = (rawInstruction << 16)
                | (rawInstruction >> 16);

        char hexStr[12]; // Buffer for "0x0000.0000" formatted string
        itoh_be(ntohsInstruction, hexStr);
        putsUart0("Faulting PC Address: ");
        putsUart0(hexStr);
        putsUart0("\n\n");
    }

    if (flags & PRINT_DATA_ADDRESSES)
    {
        // print data addresses
        putsUart0("--- DATA ADDRESSES ---\n");

        // Check if the MMFAR contains a valid address
        if (NVIC_FAULT_STAT_R & NVIC_FAULT_STAT_MMARV)
        {
            uint32_t faultingDataAddress = NVIC_MM_ADDR_R;
            char addrStr[12];
            itoh_be(faultingDataAddress, addrStr);

            putsUart0("Faulting Data Address (MMFAR): ");
            putsUart0(addrStr);
            putsUart0("\n\n");
        }
        else
        {
            putsUart0("No valid data address was recorded for this fault.\n\n");
        }

    }
}



