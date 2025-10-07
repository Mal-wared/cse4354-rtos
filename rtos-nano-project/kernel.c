//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include "kernel.h"
#include <stdint.h>
#include "uart0.h"
#include "util.h"
#include "stackHelper.h"
#include "kernel.h"
#include "tm4c123gh6pm.h"

#define MPU_REGION_COUNT 28     // defines maximum number of regions in MPU
#define MPU_REGION_SIZE_B 1024  // defines size in bytes of an MPU region

#define MPU_REGIONS_FLASH 1
#define MPU_REGIONS_PERIPHERALS 2
#define MPU_REGIONS_SRAM_START 3
#define MPU_REGIONS_SRAM_REGIONS 4

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

uint32_t pid = 10000;
uint64_t mask;
// 28 KB / 32 KB (DYNAMIC HEAP ALLOCATION/OS RESERVE SPLIT)
volatile uint8_t heap[MPU_REGION_COUNT * MPU_REGION_SIZE_B];

// Tracks which 1024-byte (1 MB) chunks are currently being used
int allocated_lengths[MPU_REGION_COUNT] = { 0 };

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// page 148
void MpuISR()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("MPU fault in process ");
    printPid();
    putsUart0("\n");

    uint32_t debugFlags = PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS |
    PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP |
    PRINT_DATA_ADDRESSES;
    printFaultDebug(debugFlags);
    while (1)
        ;
}

// page 92 - memory model: table 2-4 memory map
// page 126 - updating an MPU region
void triggerMpuFault()
{

}

// page 149
void BusISR()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Bus fault in process ");
    printPid();
    putsUart0("\n");

    while (1);
}

void triggerBusFault()
{
    // tries to access a reserved memory region, resulting in
    *(volatile uint32_t*) 0x30000000 = 0;
}

// page 150
void UsageISR()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Usage fault in process ");
    printPid();
    putsUart0("\n");
    uint32_t debugFlags = PRINT_MFAULT_FLAGS;
    printFaultDebug(debugFlags);
    while (1)
        ;
}

void triggerUsageFault()
{
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

    uint32_t debugFlags = PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS |
    PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP;
    printFaultDebug(debugFlags);
    while (1);
}

void triggerHardFault()
{
    // Disables UsageFault handler to force fault escalation
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_USAGE;
    volatile int x = 10;
    volatile int y = 0;

    // Divide-by-zero trap generates a UsageFault. Since its handler is disabled,
    // it escalates to HardFault handler.
    putSomethingIntoR0();
    volatile int z = x / y;
}

void PendSvIsr()
{
    putsUart0("--- FAULT DIAGNOSTICS ---\n");
    putsUart0("Pendsv in process ");
    printPid();
    putsUart0("\n");

    uint32_t debugFlags = PRINT_STACK_POINTERS | PRINT_MFAULT_FLAGS |
    PRINT_OFFENDING_INSTRUCTION | PRINT_STACK_DUMP;
    printFaultDebug(debugFlags);
    while (1);
}

void triggerPendSvFault()
{
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}

// Dynamically allocates SRAM memory, rounding up to the nearest 1024 byte multiple
void* malloc_heap(int size_in_bytes)
{
    // Handles zero-case
    if (size_in_bytes == 0)
    {
        return NULL;
    }

    // 1 byte needs 1 chunk, 1024 bytes needs 1 chunk, 1025 bytes needs 2 chunks
    int chunks_required = ((size_in_bytes - 1) / MPU_REGION_SIZE_B) + 1;

    // implemented "fi4st-fit" algorithm, finding first available consecutive blocks
    int i = 0;
    int j = 0;
    for (i = 0; i <= MPU_REGION_COUNT - chunks_required; i++)
    {
        bool free_block = true;

        // checks from i to i + j to see if consecutive blocks are available
        for (j = 0; j < chunks_required; j++)
        {
            if (allocated_lengths[i + j] != 0)
            {
                free_block = false;
                i = i + j;
                break; // block is not free, break from inner loop
            }
        }

        if (free_block)
        {
            allocated_lengths[i] = chunks_required;
            // allocate taken attribute to chunks via is_chunk_allocated array
            for (j = 1; j < chunks_required; j++)
            {
                allocated_lengths[i + j] = -1;
            }

            // returns actual memory
            void *ptr = (void*) (heap + (i * MPU_REGION_SIZE_B));
            uint32_t allocated_size = chunks_required * MPU_REGION_SIZE_B;


            // Update the bitmask to grant access to the new memory window
            addSramAccessWindow(&mask, (uint32_t*)ptr, allocated_size);

            // Apply the newly modified bitmask to the MPU hardware
            applySramAccessMask(mask);
            return ptr;
        }
    }

    return NULL;
}

void* free_heap(void *p)
{
    // Checks if pointer is NULL
    if (p == NULL)
    {
        return;
    }

    // Checks if memory address is outside of SRAM
    if ((uint8_t*) p < heap
            || (uint8_t*) p >= (heap + MPU_REGION_COUNT * MPU_REGION_SIZE_B))
    {
        return;
    }

    // Calculate starting index of allocated memory
    int start_allocation_index = ((uint8_t*) p - heap) / MPU_REGION_SIZE_B;

    // If the pointer doesn't correspond to the start of an active allocation,
    //   then exit function
    if (allocated_lengths[start_allocation_index] <= 0)
    {
        return;
    }

    // Get the number of chunks that needs to be freed
    int allocation_length = allocated_lengths[start_allocation_index];
    uint32_t size_to_free = allocation_length * MPU_REGION_SIZE_B;
    revokeSramAccessWindow(&mask, (uint32_t*)p, size_to_free);
    applySramAccessMask(mask);

    // Free desginated chunks
    int i = 0;
    for (i = 0; i < allocation_length; i++)
    {
        allocated_lengths[start_allocation_index + i] = 0;
    }

    // revoke SRAM access
    return;
}

// background rule, -1?
// privilege, bit 2 of MPUCTL, turning on background rule
// unles protecting flash, xn not set
//  device memory map table 2.4/92
void allowFlashAccess(void)
{
    __asm(" ISB");
    const unsigned int region = MPU_REGIONS_FLASH;

    // NVIC_MPU_NUMBER_S == number of bits to shift
    // NVIC_MPU_NUMBER_M == bitmask to not affect other register bits

    // MPUBASE - page 190
    // MPUNUMBER - page 189
    // MPUATTR - page 192

    // --- Select MPU Region to configure (FLASH - Region 1 of 8 (1/8)) ---

    // By disbaling the valid bit in the MPUBASE register,
    // we have to write directly to the MPUNUMBER register to set the region
    // OPTIONALLY: could set the valid bit and write directly to the
    //              REGION field in the MPUBASE register, but this is
    //              a nonstandard operation
    NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_VALID;

    // Clear region selection
    NVIC_MPU_NUMBER_R &= ~NVIC_MPU_NUMBER_M;

    // Select FLASH (1/8) region
    NVIC_MPU_NUMBER_R |= (region << NVIC_MPU_NUMBER_S) & NVIC_MPU_NUMBER_M; // select region /189

    // Disables the region specified in MPUNUMBER (FLASH region)
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_ENABLE;

    // Clears the ADDR field in MPUBASE
    NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_ADDR_M;

    // Sets the base address for specified memory block (FLASH)
    NVIC_MPU_BASE_R |= 0 & NVIC_MPU_BASE_ADDR_M;

    // Clears the SIZE field of the MPUATTR register
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SIZE_M;

    // Size of FLASH: 0x0000.0000 - 0x0003.FFFF = 262144 Bytes
    // MPUATTR SIZE field calculation:
    // (Region size in bytes) = 2^(SIZE+1)
    // log(Region size in bytes) = (SIZE+1)log(2)
    // log(262144) = (SIZE+1)log(2)
    // log(262144)/log(2) = (SIZE+1)
    // SIZE+1 = 18
    // SIZE = 17

    // Set the SIZE field of the MPUATTR register
    // (17 << 1) to shift out of the ENABLE field and into the SIZE field
    NVIC_MPU_ATTR_R |= (17 << 1) & NVIC_MPU_ATTR_SIZE_M;

    // Setting:
    //  Table 3-3 (Page 129): TEX, S, C, and B Bit Field Encoding
    //  Table 3-4 (Page 129): Cache Policy for Memory Attribute Encoding
    //  Table 3-5 (Page 129): AP Bit Field Encoding
    // As defined in:
    //  Table 3-6 (Page 130): Memory Region Attributes for Tiva™ C Series Microcontrollers
    // For: Flash Memory RWX
    // TEX: 0b000
    // S:   0
    // C:   1
    // B:   0
    // AP:  0b011 (Full Access)
    // XN:  0
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_TEX_M;                // Clear TEX field
    NVIC_MPU_ATTR_R |= (0b000 << 19) & NVIC_MPU_ATTR_TEX_M; // Set TEX field
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SHAREABLE;            // Set S field
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_CACHEABLE;             // Set C field
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_BUFFRABLE;            // Set B field
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_XN;                   // Set XN field
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_AP_M;     // Clear AP field
    NVIC_MPU_ATTR_R |= (0b011 & 0b111) << 24;   // Set AP field (0b111 mask)

    // Enable SRD bit - FLASH region is dominant, don't let regions
    //                  overlap or overwrite FLASH
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SRD_M;

    // Disable subregions
    // NVIC_MPU_ATTR_R |= (0b1111'1111 & 0xFF) << 8;

    // Enable region defined in MPUNUMBER
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_ENABLE;    // enable region /193

    __asm(" ISB");
}

void allowPeripheralAccess(void)
{
    __asm(" ISB");
    const unsigned int region = MPU_REGIONS_PERIPHERALS;

    // NVIC_MPU_NUMBER_S == number of bits to shift
    // NVIC_MPU_NUMBER_M == bitmask to not affect other register bits

    // MPUBASE - page 190
    // MPUNUMBER - page 189
    // MPUATTR - page 192

    // --- Select MPU Region to configure (PERIPHERALS - Region 2 of 8 (2/8)) ---

    // By disabling the valid bit in the MPUBASE register,
    // we have to write directly to the MPUNUMBER register to set the region
    // OPTIONALLY: could set the valid bit and write directly to the
    //              REGION field in the MPUBASE register, but this is
    //              a nonstandard operation
    NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_VALID;

    // Clear region selection
    NVIC_MPU_NUMBER_R &= ~NVIC_MPU_NUMBER_M;

    // Select PERIPHERAL (2/8) region
    NVIC_MPU_NUMBER_R |= (region << NVIC_MPU_NUMBER_S) & NVIC_MPU_NUMBER_M; // select region /189

    // Disables the region specified in MPUNUMBER (PERIPHERAL region)
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_ENABLE;

    // Clears the ADDR field in MPUBASE
    NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_ADDR_M;

    // Sets the base address for specified memory block (PERIPHERAL)
    NVIC_MPU_BASE_R |= 0x40000000 & NVIC_MPU_BASE_ADDR_M;

    // Clears the SIZE field of the MPUATTR register
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SIZE_M;

    // Initial Thoughts on Size:
    // - Thought the idea was to create a memory region over
    //      the entirety of peripherals to the private peripheral
    //      (0x4000.0000 - 0xFFFF.FFFF = 402653184 Bytes)
    //      bus, but we only need the memory region from the
    //      peripheral bit-band region to the peripheral bit-band alias

    // Size of PERIPHERAL: 0x4000.0000 - 0x43FF.FFFF = 67108864 Bytes
    // MPUATTR SIZE field calculation:
    // (Region size in bytes) = 2^(SIZE+1)
    // log(Region size in bytes) = (SIZE+1)log(2)
    // log(67108864) = (SIZE+1)log(2)
    // log(67108864)/log(2) = (SIZE+1)
    // SIZE+1 = 26
    // SIZE = 25

    // Set the SIZE field of the MPUATTR register
    // (17 << 1) to shift out of the ENABLE field and into the SIZE field
    NVIC_MPU_ATTR_R |= (25 << 1) & NVIC_MPU_ATTR_SIZE_M;

    // Setting:
    //  Table 3-3 (Page 129): TEX, S, C, and B Bit Field Encoding
    //  Table 3-4 (Page 129): Cache Policy for Memory Attribute Encoding
    //  Table 3-5 (Page 129): AP Bit Field Encoding
    // As defined in:
    //  Table 3-6 (Page 130): Memory Region Attributes for Tiva™ C Series Microcontrollers
    // For: Peripheral Memory RW Unprivileged and Privileged
    // TEX: 0b000
    // S:   1
    // C:   0
    // B:   1
    // AP:  0b011 (Full Access)
    // XN:  1 (not executable code)
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_TEX_M;                // Clear TEX field
    NVIC_MPU_ATTR_R |= (0b000 << 19) & NVIC_MPU_ATTR_TEX_M; // Set TEX field
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_SHAREABLE;             // Set S field
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_CACHEABLE;            // Set C field
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_BUFFRABLE;             // Set B field
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_XN;                   // Set XN field
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_AP_M;     // Clear AP field
    NVIC_MPU_ATTR_R |= (0b011 & 0b111) << 24;   // Set AP field (0b111 mask)

    // Enable SRD bit - PERIPHERAL region is dominant, don't let regions
    //                  overlap or overwrite PERIPHERAL
    // 0 for all 8 bits on the SRD field means it enables all subregions
    NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SRD_M;

    // Disable subregions
    // NVIC_MPU_ATTR_R |= (0b1111'1111 & 0xFF) << 8;

    // Enable region defined in MPUNUMBER
    NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_ENABLE;    // enable region /193

    __asm(" ISB");
}

void setupSramAccess(void)
{
    __asm(" ISB");

    int i;
    for (i = 0; i < MPU_REGIONS_SRAM_REGIONS; i++)
    {
        // NVIC_MPU_NUMBER_S == number of bits to shift
        // NVIC_MPU_NUMBER_M == bitmask to not affect other register bits

        // MPUBASE - page 190
        // MPUNUMBER - page 189
        // MPUATTR - page 192

        // --- Select MPU Region to configure (PERIPHERALS - Region 2 of 8 (2/8)) ---

        // By disabling the valid bit in the MPUBASE register,
        // we have to write directly to the MPUNUMBER register to set the region
        // OPTIONALLY: could set the valid bit and write directly to the
        //              REGION field in the MPUBASE register, but this is
        //              a nonstandard operation
        NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_VALID;

        // Clear region selection
        NVIC_MPU_NUMBER_R &= ~NVIC_MPU_NUMBER_M;

        // Select SRAM (3-6/8 - 4 regions total) region
        NVIC_MPU_NUMBER_R |= ((MPU_REGIONS_SRAM_START + i) << NVIC_MPU_NUMBER_S)
                & NVIC_MPU_NUMBER_M;

        // Disables the region specified in MPUNUMBER (SRAM region)
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_ENABLE;

        // Clears the ADDR field in MPUBASE
        NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_ADDR_M;

        // Sets the base address for specified memory block (PERIPHERAL)
        NVIC_MPU_BASE_R |= (0x20000000 + 0x00002000 * i) & NVIC_MPU_BASE_ADDR_M;

        // Clears the SIZE field of the MPUATTR register
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SIZE_M;

        // Size of PERIPHERAL: 0x2000.0000 - 0x2000.1FFF = 8192 Bytes
        // MPUATTR SIZE field calculation:
        // (Region size in bytes) = 2^(SIZE+1)
        // log(Region size in bytes) = (SIZE+1)log(2)
        // log(8192) = (SIZE+1)log(2)
        // log(8192)/log(2) = (SIZE+1)
        // SIZE+1 = 13
        // SIZE = 12

        // Set the SIZE field of the MPUATTR register
        // << 1 to shift out of the ENABLE field and into the SIZE field
        NVIC_MPU_ATTR_R |= (12 << 1) & NVIC_MPU_ATTR_SIZE_M;

        // Setting:
        //  Table 3-3 (Page 129): TEX, S, C, and B Bit Field Encoding
        //  Table 3-4 (Page 129): Cache Policy for Memory Attribute Encoding
        //  Table 3-5 (Page 129): AP Bit Field Encoding
        // As defined in:
        //  Table 3-6 (Page 130): Memory Region Attributes for Tiva™ C Series Microcontrollers
        // For: SRAM Memory RW Privileged & No Access Unprivileged
        // TEX: 0b000
        // S:   0
        // C:   1
        // B:   0
        // AP:  0b011 (RW Privileged & Unprivileged)
        // XN:  0 (executable code)
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_TEX_M;              // Clear TEX field
        NVIC_MPU_ATTR_R |= (0b000 << 19) & NVIC_MPU_ATTR_TEX_M; // Set TEX field
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SHAREABLE;            // Set S field
        NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_CACHEABLE;             // Set C field
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_BUFFRABLE;            // Set B field
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_XN;                   // Set XN field
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_AP_M;     // Clear AP field
        NVIC_MPU_ATTR_R |= (0b011 & 0b111) << 24;   // Set AP field (0b111 mask)

        // 0 for all 8 bits on the SRD field means it enables all subregions
        //NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SRD_M;

        // Disable subregions
        NVIC_MPU_ATTR_R |= (0b1111'1111 & 0xFF) << 8;

        // Enable region defined in MPUNUMBER
        NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_ENABLE;

        __asm(" ISB");
    }
}

uint64_t createNoSramAccessMask(void)
{
    // only 32 bits instead of 64 bc there are only 4 regions dedicated to SRAM
    return 0xFFFFFFFF;
}

void applySramAccessMask(uint64_t srdBitMask)
{
    __asm(" ISB");

    int i;
    for (i = 0; i < MPU_REGIONS_SRAM_REGIONS; i++)
    {
        // Clear VALID field for standard instruction
        NVIC_MPU_BASE_R &= ~NVIC_MPU_BASE_VALID;

        // Clear region selection
        NVIC_MPU_NUMBER_R &= ~NVIC_MPU_NUMBER_M;

        // Select SRAM (3-6/8 - 4 regions total) region
        NVIC_MPU_NUMBER_R |= ((MPU_REGIONS_SRAM_START + i) << NVIC_MPU_NUMBER_S)
                & NVIC_MPU_NUMBER_M;

        // Disables the region specified in MPUNUMBER (SRAM region)
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_ENABLE;

        // Clear SRD bits
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SRD_M;

        // Apply bitmask to SRD bits
        NVIC_MPU_ATTR_R |= ((srdBitMask >> 8 * i) & 0xFF) << 8;

        // Enable region defined in MPUNUMBER
        NVIC_MPU_ATTR_R |= NVIC_MPU_ATTR_ENABLE;

    }
}

void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes)
{

    if (size_in_bytes == 0 || srdBitMask == NULL || ((uint32_t) baseAdd < 0x20000000))
    {
        return;
    }

    if (((uint32_t) baseAdd + size_in_bytes) > 0x20008000)
    {
        size_in_bytes = 0x20008000 - (uint32_t) baseAdd;
    }

    unsigned int subregionStartIndex = ((uint32_t) baseAdd - 0x20000000) / 1024;
    unsigned int additionalSubregions = (size_in_bytes - 1) / 1024;
    int i;
    for (i = 0; i <= additionalSubregions; i++)
    {
        // add access by disabling the rule
        // 1ULL = unsigned long long (64 bit) int with value of 1
        *srdBitMask &= ~(1ULL << (subregionStartIndex + i));
    }
}

void revokeSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes)
{
    if (size_in_bytes == 0 || srdBitMask == NULL || ((uint32_t) baseAdd < 0x20000000))
    {
        return;
    }

    if (((uint32_t) baseAdd + size_in_bytes) > 0x20008000)
    {
        size_in_bytes = 0x20008000 - (uint32_t) baseAdd;
    }

    unsigned int subregionStartIndex = ((uint32_t) baseAdd - 0x20000000) / 1024;
    unsigned int additionalSubregions = (size_in_bytes - 1) / 1024;
    int i;
    for (i = 0; i <= additionalSubregions; i++)
    {
        // Revoke access by enabling the rule (setting the bit to 1)
        *srdBitMask |= (1ULL << (subregionStartIndex + i));
    }
}

void setupMPU()
{
    setupSramAccess();
    mask = createNoSramAccessMask();
    addSramAccessWindow(&mask, (uint32_t *)0x20000000, 32768);
    applySramAccessMask(mask);
}

// Add these functions to shell.c

void comprehensiveMPUTest()
{
    // --- PART 1: PRIVILEGED SETUP AND ACCESS ---
    putsUart0("--- Comprehensive MPU Test ---\n");
    putsUart0("PRIVILEGED: Allocating 1024 bytes with malloc_heap...\n");

    // Allocate memory. This also opens an MPU access window for this 1KB block.
    volatile uint32_t *p = malloc_heap(1024);
    if (p == NULL)
    {
        putsUart0("FAILURE: malloc_heap returned NULL.\n");
        return;
    }

    putsUart0("PRIVILEGED: Write/read test to allocated SRAM...\n");
    *p = 0x12345678; // Write to the memory
    if (*p == 0x12345678)
    {
        putsUart0("SUCCESS: Privileged SRAM access verified. \n\n");
    }
    else
    {
        putsUart0("FAILURE: Privileged SRAM access failed. \n\n");
    }

    // --- PART 2: UNPRIVILEGED ACCESS (SHOULD SUCCEED) ---
    putsUart0("SWITCHING to Unprivileged Mode...\n");
    setTMPL();

    putsUart0("UNPRIVILEGED: Verifying peripheral and flash access...\n");
    // This function call proves flash access (running code) and peripheral access (writing to UART).
    putsUart0("SUCCESS: Unprivileged peripheral/flash access verified. \n");

    putsUart0("UNPRIVILEGED: Write/read test to the same allocated SRAM...\n");
    *p = 0xABCDEF01; // Write to the memory again
    if (*p == 0xABCDEF01)
    {
        putsUart0("SUCCESS: Unprivileged SRAM access verified. \n\n");
    }
    else
    {
        putsUart0("FAILURE: Unprivileged SRAM access failed. \n\n");
    }

    // --- PART 3: UNPRIVILEGED ACCESS (SHOULD FAULT) ---
    putsUart0("UNPRIVILEGED: Verifying protection of SRAM outside the allocated range.\n");
    putsUart0("This next step is EXPECTED to cause an MPU fault.\n");

    // Calculate a pointer to the next, unallocated (and thus protected) 1KB block
    volatile uint32_t *bad_p = p + (1024 / sizeof(uint32_t));
    *bad_p = 0xBADF00D; // This line should trigger the MpuISR

    // If the code reaches this point, the MPU is not configured correctly.
    putsUart0("FAILURE: MPU did not protect memory outside the allocated range!\n");
}

void testMpuAfterFree()
{
    // --- PART 1: PRIVILEGED SETUP AND FREE ---
    putsUart0("--- MPU Post-Free Test ---\n");
    putsUart0("PRIVILEGED: Allocating and immediately freeing 1024 bytes...\n");

    volatile uint32_t *p = malloc_heap(1024);
    if (p == NULL)
    {
        putsUart0("FAILURE: malloc_heap returned NULL.\n");
        return;
    }

    // Free the memory. This should revoke MPU access to the 1KB block.
    free_heap((void*)p);
    putsUart0("PRIVILEGED: Memory freed and MPU access revoked.\n\n");


    // --- PART 2: UNPRIVILEGED ACCESS (SHOULD FAULT) ---
    putsUart0("SWITCHING to Unprivileged Mode...\n");
    setTMPL();

    putsUart0("UNPRIVILEGED: Attempting to access the freed memory...\n");
    putsUart0("This next step is EXPECTED to cause an MPU fault.\n");

    *p = 0xBADF00D; // This line should trigger the MpuISR

    // If the code reaches this point, the MPU is not configured correctly.
    putsUart0("FAILURE: MPU did not protect memory after it was freed! \n");
}

void testSRAM()
{
    uint32_t *pointers[12];
    uint32_t malloc_regions[10] = {512, 1024, 512, 1536, 1024, 1024, 1024, 1024, 512, 4096};
    int i;
    for (i = 0; i < 10; i++) {
        pointers[i] = malloc_heap(malloc_regions[i]);
    }
    //free_heap(pointers[2]);
    applySramAccessMask(mask);
    setTMPL();
    *pointers[2] = 10;

}

void printPid()
{
    // String of size 12 for 10 digits (4,294,967,295) + 1 sign + 1 null terminator
    char pidStr[12];
    itoa(pid, pidStr);
    putsUart0(pidStr);
    putsUart0("\n");
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

        // 3. Read the 16-bit instruction value directly from that memory address.

        char hexStr[12]; // Buffer for "0x0000.0000" formatted string
        itoh((uint32_t) *faultingPc, hexStr);
        putsUart0(hexStr);
        putsUart0(": ");
    }

    if (flags & PRINT_DATA_ADDRESSES)
    {
        // print data addresses
        putsUart0("--- DATA ADDRESSES ---\n");
    }
}
