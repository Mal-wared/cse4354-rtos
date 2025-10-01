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
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("MPU fault in process ");
    printPid();
    putsUart0("\n");

    uint32_t debugFlags =    PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS |
                            PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP |
                            PRINT_DATA_ADDRESSES;
    printFaultDebug(debugFlags);
    while(1);
}

// page 92 - memory model: table 2-4 memory map
// page 126 - updating an MPU region
void triggerMpuFault() {
    // A pointer to a valid SRAM address
        volatile uint32_t* protected_address = (uint32_t*)0x20004000;

        // Configure an MPU region

        // Use MPU region 0, disable it first
        NVIC_MPU_NUMBER_R = 0;
        NVIC_MPU_ATTR_R = 0;

        // Set the base address of the region (must be aligned to size)
        NVIC_MPU_BASE_R = 0x20004000;

        // Configure the region attributes:
        // - Enable region
        // - 1 KiB size
        // - Privileged: Read/Write, Unprivileged: Read-Only
        NVIC_MPU_ATTR_R = NVIC_MPU_ATTR_XN |      // No-execute
                          NVIC_MPU_ATTR_AP_PRW_UR |// Priv RW, Unpriv RO
                          (9 << 1) |             // Region size 2^(9+1) = 1024 bytes
                          NVIC_MPU_ATTR_ENABLE;

        // --- Step 2: Enable the MPU ---
        NVIC_MPU_CTRL_R = NVIC_MPU_CTRL_ENABLE | NVIC_MPU_CTRL_PRIVDEFEN;

        __asm(" DSB"); // Data Synchronization Barrier
        __asm(" ISB"); // Instruction Synchronization Barrier

        // --- Step 3: Switch to Unprivileged Mode ---
        switchToUnprivileged();

        // --- Step 4: Violate the Rule ---
        // This line will now trigger an MPU fault because unprivileged
        // code is attempting to WRITE to a READ-ONLY location.
        *protected_address = 0xDEADBEEF;

}

// page 149
void BusISR()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Bus fault in process ");
    printPid();
    putsUart0("\n");

    while(1);
}

void triggerBusFault() {
    // tries to access a reserved memory region, resulting in
    *(volatile uint32_t *)0x30000000 = 0;
}

// page 150
void UsageISR()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Usage fault in process ");
    printPid();
    putsUart0("\n");
    uint32_t debugFlags =    PRINT_MFAULT_FLAGS;
    printFaultDebug(debugFlags);
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
    while(1);
}

void triggerHardFault() {
    // Disables UsageFault handler to force fault escalation
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_USAGE;
    volatile int x = 10;
    volatile int y = 0;

    // Divide-by-zero trap generates a UsageFault. Since its handler is disabled,
    // it escalates to HardFault handler.
    putSomethingIntoR0();
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
    uint32_t * currentPsp = getPsp();
    uint32_t * currentMsp = getMsp();

    // Print PSP and MSP addresses
    if (flags & PRINT_STACK_POINTERS) {
        char mspStr[12];
        itoh((uint32_t)currentMsp, mspStr);

        char pspStr[12];
        itoh((uint32_t)currentPsp, pspStr);

        putsUart0("--- STACK POINTERS ---\n");
        putsUart0("MSP    (Main Stack Pointer):\t");
        putsUart0(mspStr);
        putsUart0("\n");

        putsUart0("PSP (Process Stack Pointer):\t");
        putsUart0(pspStr);
        putsUart0("\n\n");
    }

    if (flags & PRINT_STACK_DUMP) {
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
    if (flags & PRINT_MFAULT_FLAGS) {
        uint32_t faultStat = NVIC_FAULT_STAT_R;
        putsUart0("--- FAULT STATUS REGISTERS ---\n");
        putsUart0("Indicated causes of fault(s) in FAULTSTAT register:\n");
        if(faultStat & NVIC_FAULT_STAT_DIV0){
            putsUart0("- [BIT 25] Divide-by-Zero Usage Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_UNALIGN){
            putsUart0("- [BIT 24] Unaligned Access Usage Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_NOCP){
            putsUart0("- [BIT 19] No Coprocessor Usage Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_INVPC){
            putsUart0("- [BIT 18] Invalid PC Load Usage Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_INVSTAT){
            putsUart0("- [BIT 17] Invalid State Usage Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_UNDEF){
            putsUart0("- [BIT 16] Undefined Instruction Usage Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_BFARV){
            putsUart0("- [BIT 15] Bus Fault Address Register Valid\n");
        }
        if(faultStat & NVIC_FAULT_STAT_BLSPERR){
            putsUart0("- [BIT 13] Bus Fault on Floating-Point Lazy State Preservation\n");
        }
        if(faultStat & NVIC_FAULT_STAT_BSTKE){
            putsUart0("- [BIT 12] Stack Bus Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_BUSTKE){
            putsUart0("- [BIT 11] Unstack Bus Fault\n");
        }
        if(faultStat & NVIC_FAULT_STAT_IMPRE){
            putsUart0("- [BIT 10] Imprecise Data Bus Error\n");
        }
        if(faultStat & NVIC_FAULT_STAT_PRECISE){
            putsUart0("- [BIT 9] Precise Data Bus Error\n");
        }
        if(faultStat & NVIC_FAULT_STAT_IBUS){
            putsUart0("- [BIT 8] Instruction Bus Error\n");
        }
        if(faultStat & NVIC_FAULT_STAT_MMARV){
            putsUart0("- [BIT 7] Memory Management Fault Address Register Valid\n");
        }
        if(faultStat & NVIC_FAULT_STAT_MLSPERR){
            putsUart0("- [BIT 5] Memory Management Fault on Floating-Point Lazy State Preservation\n");
        }
        if(faultStat & NVIC_FAULT_STAT_MSTKE){
            putsUart0("- [BIT 4] Stack Access Violation (Memory Management Fault)\n");
        }
        if(faultStat & NVIC_FAULT_STAT_MUSTKE){
            putsUart0("- [BIT 3] Unstack Access Violation (Memory Management Fault)\n");
        }
        if(faultStat & NVIC_FAULT_STAT_DERR){
            putsUart0("- [BIT 1] Data Access Violation\n");
        }
        if(faultStat & NVIC_FAULT_STAT_IERR){
            putsUart0("- [BIT 0] Instruction Access Violation\n");
        }
        putsUart0("\n");

    }

    // page 75 figure 2-3 cortex-m4f register set
    if (flags & PRINT_OFFENDING_INSTRUCTION) {
        // print offending instruction
        putsUart0("--- OFFENDING INSTRUCTION ---\n");
        uint32_t faultingPc = currentPsp[6];
        uint16_t* instructionAddress = (uint16_t*)(faultingPc - 2);

        // 3. Read the 16-bit instruction value directly from that memory address.
        uint16_t instructionValue = *instructionAddress;

        char hexStr[12]; // Buffer for "0x0000.0000" formatted string
        itoh((uint32_t)currentPsp[6], hexStr);
        putsUart0(hexStr);
        putsUart0(": ");
    }

    if (flags & PRINT_DATA_ADDRESSES) {
        // print data addresses
        putsUart0("--- DATA ADDRESSES ---\n");
    }
}
