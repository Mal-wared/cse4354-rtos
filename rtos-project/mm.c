// Nicholas Nhat Tran
// 1002027150

// Memory manager functions
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
#include <stddef.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "mm.h"

#define MPU_REGION_COUNT 28     // defines maximum number of regions in MPU
#define MPU_REGION_SIZE_B 1024  // defines size in bytes of an MPU region

#define MPU_REGIONS_FLASH 1
#define MPU_REGIONS_PERIPHERALS 2
#define MPU_REGIONS_SRAM_START 3
#define MPU_REGIONS_SRAM_REGIONS 4

uint64_t mask;

volatile uint8_t heap[MPU_REGION_COUNT * MPU_REGION_SIZE_B] __attribute__((aligned(1024)));

// Tracks which 1024-byte (1 MB) chunks are currently being used
int allocated_lengths[MPU_REGION_COUNT] = { 0 };

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// REQUIRED: add your malloc code here and update the SRD bits for the current thread
void* mallocHeap(uint32_t size_in_bytes)
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
            addSramAccessWindow(&mask, (uint32_t*) ptr, allocated_size);

            // Apply the newly modified bitmask to the MPU hardware
            applySramAccessMask(mask);
            return ptr;
        }
    }

    return NULL;
}

// REQUIRED: add your free code here and update the SRD bits for the current thread
void freeHeap(void *address_from_malloc)
{
    // Checks if pointer is NULL
    if (address_from_malloc == NULL)
    {
        return;
    }

    // Checks if memory address is outside of SRAM
    if ((uint8_t*) address_from_malloc < heap
            || (uint8_t*) address_from_malloc
                    >= (heap + MPU_REGION_COUNT * MPU_REGION_SIZE_B))
    {
        return;
    }

    // Calculate starting index of allocated memory
    int start_allocation_index = ((uint8_t*) address_from_malloc - heap)
            / MPU_REGION_SIZE_B;

    // If the pointer doesn't correspond to the start of an active allocation,
    //   then exit function
    if (allocated_lengths[start_allocation_index] <= 0)
    {
        return;
    }

    // Get the number of chunks that needs to be freed
    int allocation_length = allocated_lengths[start_allocation_index];
    uint32_t size_to_free = allocation_length * MPU_REGION_SIZE_B;
    revokeSramAccessWindow(&mask, (uint32_t*) address_from_malloc,
                           size_to_free);
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
    //  Table 3-6 (Page 130): Memory Region Attributes for Tiva C Series Microcontrollers
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
    //  Table 3-6 (Page 130): Memory Region Attributes for Tiva  C Series Microcontrollers
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
        //  Table 3-6 (Page 130): Memory Region Attributes for Tiva  C Series Microcontrollers
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

void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd,
                         uint32_t size_in_bytes)
{

    if (size_in_bytes == 0 || srdBitMask == NULL
            || ((uint32_t) baseAdd < 0x20000000))
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

void revokeSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd,
                            uint32_t size_in_bytes)
{
    if (size_in_bytes == 0 || srdBitMask == NULL
            || ((uint32_t) baseAdd < 0x20000000))
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

// REQUIRED: add code to initialize the memory manager
void initMemoryManager(void)
{
    mask = createNoSramAccessMask();
    //    addSramAccessWindow(&mask, (uint32_t *)0x20000000, 32768);
    applySramAccessMask(mask);
}

// REQUIRED: add your custom MPU functions here (eg to return the srd bits)

// REQUIRED: initialize MPU here
void initMpu(void)
{
    // REQUIRED: call your MPU functions here
    allowFlashAccess();
    allowPeripheralAccess();
    setupSramAccess();
    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_ENABLE | NVIC_MPU_CTRL_PRIVDEFEN;
}

