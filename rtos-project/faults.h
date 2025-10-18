// Faults functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef FAULTS_H_
#define FAULTS_H_

#define PRINT_STACK_POINTERS        (1 << 0)
#define PRINT_MFAULT_FLAGS          (1 << 1)
#define PRINT_OFFENDING_INSTRUCTION (1 << 2)
#define PRINT_DATA_ADDRESSES        (1 << 3)
#define PRINT_STACK_DUMP            (1 << 4)

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void mpuFaultIsr(void);
void triggerMpuFault();

void hardFaultIsr(void);
void triggerHardFault();

void busFaultIsr(void);
void triggerBusFault();

void usageFaultIsr(void);
void triggerUsageFault();

void printFaultDebug(uint32_t flags);

#endif
