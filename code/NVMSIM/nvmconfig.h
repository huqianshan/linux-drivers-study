/**
 * NVMSIM related configuration
 */

#ifndef __NVMCONFIG_H

#define NVM_CONFIG_VMALLOC 0 /* use vmalloc() to allocate memory*/
#define NVM_CONFIG_HIGHMEM 1 /* use ioremap to map highmemory-based memory*/
/**
 * NVM operation codes
 */
#define NVM_READ 0
#define NVM_WRITE 1

unsigned g_nvm_type = NVM_CONFIG_HIGHMEM;

#define NVM_USE_HIGHMEM() (g_nvm_type == NVM_CONFIG_HIGHMEM)

/* high memory configs */
unsigned long g_highmem_size = 0;    /* size of the reserved physical mem space (bytes) */
phys_addr_t g_highmem_phys_addr = 0x100000000; /* beginning of the reserved phy mem space (bytes)*/
void *g_highmem_virt_addr = NULL;    /* beginning of the reserve HIGH_MEM space */
void *g_highmem_curr_addr = NULL;    /* beginning of the available HIGH_MEM space for alloc*/

/* high memory */
#define PMBD_HIGHMEM_AVAILABLE_SPACE (g_highmem_virt_addr + g_highmem_size - g_highmem_curr_addr)

/**
 *  The SIZE TRANSFER
 */
#define SECTOR_SHIFT 9
#define KB_SHIFT 10
#define MB_SHIFT 20
#define GB_SHIFT 30
#define MB_TO_BYTES(N) ((N) << MB_SHIFT)
#define GB_TO_BYTES(N) ((N) << GB_SHIFT)
#define BYTES_TO_MB(N) ((N) >> MB_SHIFT)
#define BYTES_TO_GB(N) ((N) >> GB_SHIFT)
#define MB_TO_SECTORS(N) ((N) << (MB_SHIFT - SECTOR_SHIFT))
#define GB_TO_SECTORS(N) ((N) << (GB_SHIFT - SECTOR_SHIFT))
#define SECTORS_TO_MB(N) ((N) >> (MB_SHIFT - SECTOR_SHIFT))
#define SECTORS_TO_GB(N) ((N) >> (GB_SHIFT - SECTOR_SHIFT))
#define SECTOR_TO_PAGE(N) ((N) >> (PAGE_SHIFT - SECTOR_SHIFT))
#define SECTOR_TO_BYTE(N) ((N) << SECTOR_SHIFT)
#define BYTE_TO_SECTOR(N) ((N) >> SECTOR_SHIFT)


#define __NVMCONFIG_H
#endif
