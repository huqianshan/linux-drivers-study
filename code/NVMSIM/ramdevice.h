/*
 * ramDevice.h
 * NVM Simulator: RAM backed block device driver
 * 
 */

#ifndef __RAMDEVICE_H
#define __RAMDEVICE_H


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
uint64_t g_highmem_size = 0;    /* size of the reserved physical mem space (bytes) */

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




#define NVM_MAJOR 231
#define SECTOR_BYTES_SHIFT 9
#define PAGE_SECTORS_SHIFT (PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS (1 << PAGE_SECTORS_SHIFT)

#define HARDSECT_SIZE (512)
#define KERNEL_SECT_SIZE (512)

#define MB_PER_BYTES_SHIFT (20)
#define MB_PER_SECTOR_SHIFT (11)

#define PARTION_PER_DISK (1)

#define NVMDEV_MEM_MAX_SECTORS (8)
#define NVM_RAMDISK_ONLY (1)

/**
 * The simulated NVM device with  RAM 
 */
struct nvm_device
{

	int nvmdev_number;					// The device number
	unsigned long nvmdev_capacity;		// The capacity in sectors BUG should in bytes?
	u8 *nvmdev_data;					// The backing data store
	spinlock_t nvmdev_lock;				// The lock protecting the data store
	struct request_queue *nvmdev_queue; /// Request queue
	struct gendisk *nvmdev_disk;		/// Disk

	struct list_head nvmdev_list; /// The collection of lists the device belongs to
};

/**
 * Allocate and free the NVM device
 */
struct nvm_device *nvm_alloc(int index, unsigned capacity_mb);
void nvm_free(struct nvm_device *device);

/**
 *  NOTE: we can also use ioremap_* functions to directly set memory
 *  page attributes when do remapping,
 */
void *nvm_highmem_map(void);
void nvm_highmem_unmap(void);
static void *hmalloc(uint64_t bytes);
static int hfree(void *addr)
{
	/* FIXME: no support for dynamic alloc/dealloc in HIGH_MEM space */
	return 0;
}

/**
 * Binder requet to queue
 */

static void nvm_make_request(struct request_queue *q, struct bio *bio);

/**
 *  nvmdev_do_bvec
 * 			Process a single request
 */
static int nvm_do_bvec(struct nvm_device *device, struct page *page,
					   unsigned int len, unsigned int off, int rw, sector_t sector);

/** 
 * Copy n bytes to from the NVM to dest starting at the given sector
 */
void __always_inline copy_from_nvm(void *dest, struct nvm_device *device,
								   sector_t sector, size_t n);

void __always_inline copy_to_nvm(struct nvm_device *device,
								 const void *src, sector_t sector, size_t n);

/**
 * Perform I/O control
 */
static int nvm_ioctl(struct block_device *bdev, fmode_t mode,
					 unsigned int cmd, unsigned long arg);

static int nvm_disk_getgeo(struct block_device *bdev,
						   struct hd_geometry *geo);

#endif