#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>

//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------
void MpuISR();
void triggerMpuFault();

void BusISR();
void triggerBusFault();

void UsageISR();
void triggerUsageFault();

void HardFaultISR();
void triggerHardFault();

void PendSvIsr();
void triggerPendSvFault();

void *malloc_heap (int size_in_bytes);
void *free_heap(void * p);
void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
uint64_t createNoSramAccessMask(void);
void applySramAccessMask(uint64_t srdBitMask);
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes);

void printPid();
void printFaultDebug(uint32_t flags);

#endif
