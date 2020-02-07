/*
 * ramDevice.C
 * NVM Simulator: RAM based block device driver
 * 
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/genhd.h> // gendisk
#include <linux/bio.h>
#include <linux/blkdev.h> // blk_queue_xx
#include <linux/fs.h>	 // block_device
#include <linux/hdreg.h>  // hd_geometry
#include <linux/blk_types.h>
#include <linux/bvec.h>
#include <asm/io.h>

#include "mem.h"
#include "ramdevice.h"

/**
 * 
 * -------Module Parameters-------
 * nvm_num_devices
 *      The maximum number of NVM devices
 * 
 * nvmdevices_mutex:
 *      The mutex guarding the list of devices
 */
static int nvm_num_devices = 1;

module_param(nvm_num_devices, int, 0);

/**
 * Size of each NVM disk in MB
 */
int nvm_capacity_mb = 4096;
uint64_t g_highmem_phys_addr = 0x100000000; /* beginning of the reserved phy mem space (bytes)*/
module_param(nvm_capacity_mb, int, 0);
MODULE_PARM_DESC(nvm_capacity_mb, "Size of each NVM disk in MB");

/**
 * The list and mutex of NVM devices
 */
static LIST_HEAD(nvm_list_head);
static DEFINE_MUTEX(nvm_devices_mutex);

/**
 * nvm_devices_name:
 *      The name of device
 */
#define NVM_DEVICES_NAME "nvm"

/**
 * NVM block device operations
 */
static const struct block_device_operations nvmdev_fops = {
	.owner = THIS_MODULE,
	.getgeo = nvm_disk_getgeo,
};

/**
 * Allocate the NVM device
 * 	  1. nvm_alloc() : allocates disk and driver 
 *    2. nvm_highmem_map() : make mapping for highmem physical address by ioremap()
 */

void *nvm_highmem_map(void)
{
	// https://patchwork.kernel.org/patch/3092221/
	if ((g_highmem_virt_addr = ioremap_cache(g_highmem_phys_addr, g_highmem_size)))
	{

		g_highmem_curr_addr = g_highmem_virt_addr;
		printk(KERN_INFO "NVMSIM: high memory space remapped (offset: %llu MB, size=%lu MB)\n",
			   BYTES_TO_MB(g_highmem_phys_addr), BYTES_TO_MB(g_highmem_size));
		return g_highmem_virt_addr;
	}
	else
	{
		printk(KERN_ERR "NVMSIM: %s(%d) %llu Bytes:%x failed remapping high memory space (offset: %llu MB size=%llu MB)\n",
			   __FUNCTION__, __LINE__, g_highmem_phys_addr, g_highmem_virt_addr, BYTES_TO_MB(g_highmem_phys_addr), BYTES_TO_MB(g_highmem_size));
		return NULL;
	}
}

void nvm_highmem_unmap(void)
{
	/* de-remap the high memory from kernel address space */
	if (g_highmem_virt_addr)
	{
		iounmap(g_highmem_virt_addr);
		g_highmem_virt_addr = NULL;
		printk(KERN_INFO "NVMSIM: unmapping high mem space (offset: %llu MB, size=%lu MB)is unmapped\n",
			   BYTES_TO_MB(g_highmem_phys_addr), BYTES_TO_MB(g_highmem_size));
	}
	return;
}

static void *hmalloc(uint64_t bytes)
{
	void *rtn = NULL;

	/* check if there is still available reserve high memory space */
	if (bytes <= PMBD_HIGHMEM_AVAILABLE_SPACE)
	{
		rtn = g_highmem_curr_addr;
		g_highmem_curr_addr += bytes;
	}
	else
	{
		printk(KERN_ERR "NVMSIM: %s(%d) - no available space (< %llu bytes) in reserved high memory\n",
			   __FUNCTION__, __LINE__, bytes);
	}
	return rtn;
}

struct nvm_device *nvm_alloc(int index, unsigned capacity_mb)
{
	struct nvm_device *device;
	struct gendisk *disk;

	// Allocate the device
	device = kzalloc(sizeof(struct nvm_device), GFP_KERNEL);
	if (!device)
		goto out;
	device->nvmdev_number = index;
	device->nvmdev_capacity = capacity_mb << MB_PER_SECTOR_SHIFT; // in Sectors
	spin_lock_init(&device->nvmdev_lock);

	// vmaloc allocate in size bytes
	if (NVM_USE_HIGHMEM())
	{
		device->nvmdev_data = hmalloc(device->nvmdev_capacity << SECTOR_BYTES_SHIFT);
	}
	else
	{
		device->nvmdev_data = vmalloc(device->nvmdev_capacity << SECTOR_BYTES_SHIFT);
	}

	if (device->nvmdev_data != NULL)
	{
#if 0
		/* FIXME: No need to do this. It's slow, system could be locked up */
		memset(pmbd->mem_space, 0, pmbd->sectors * pmbd->sector_size);
#endif
		printk(KERN_INFO "NVMSIM:  created [%lu : %llu MBs]\n",
			   (unsigned long)device->nvmdev_data, SECTORS_TO_MB(device->nvmdev_capacity));
	}
	else
	{
		printk(KERN_ERR "NVMSIM: %s(%d): NVM space allocation failed\n", __FUNCTION__, __LINE__);

		goto out_free_struct;
	}

	// Allocate the block request queue by blk_alloc_queue without I/O scheduler

	device->nvmdev_queue = blk_alloc_queue(GFP_KERNEL);
	if (!device->nvmdev_queue)
	{
		goto out_free_dev;
	}
	// register nvmdev_queue,
	blk_queue_make_request(device->nvmdev_queue, (make_request_fn *)nvm_make_request);

	//blk_queue_max_hw_sectors(device->nvmdev_queue, 255);//set max sectors for a request for this queue

	blk_queue_logical_block_size(device->nvmdev_queue, HARDSECT_SIZE); //set logical block size for the queue

	// Allocate the disk device /* cannot be partitioned */
	device->nvmdev_disk = alloc_disk(PARTION_PER_DISK);
	disk = device->nvmdev_disk;
	if (!disk)
		goto out_free_queue;
	disk->major = NVM_MAJOR;
	disk->first_minor = index;
	disk->fops = &nvmdev_fops;
	disk->private_data = device;
	disk->queue = device->nvmdev_queue;
	//disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "nvm%d", index);

	// in sectors
	set_capacity(disk, capacity_mb << MB_PER_SECTOR_SHIFT);

	return device;

	// Cleanup on error
out_free_queue:
	blk_cleanup_queue(device->nvmdev_queue);
out_free_dev:
	vfree(device->nvmdev_data);
out_free_struct:
	kfree(device);
out:
	return NULL;
}

/**
 * Free a NVM device
 */
void nvm_free(struct nvm_device *device)
{
	put_disk(device->nvmdev_disk);
	blk_cleanup_queue(device->nvmdev_queue);

	if (device->nvmdev_data != NULL)
	{
		if (NVM_USE_HIGHMEM())
		{
			hfree(device->nvmdev_data);
		}
		else
		{
			vfree(device->nvmdev_data);
		}
	}

	kfree(device);
}

/**
 * Process pending requests from the queue
 */
static void nvm_make_request(struct request_queue *q, struct bio *bio)
{
	// bio->bi_bdev has been discarded
	//struct block_device *bdev = bio->bi_bdev;
	//struct nvm_device *device = bdev->bd_disk->private_data;

	struct nvm_device *nvm_dev = bio->bi_disk->private_data;

	int rw;
	int err = -EIO;
	sector_t sector;
	unsigned capacity;

	struct bio_vec bvec;
	struct bvec_iter iter;

	// Check the device capacity
	// bi_sector,bi_size has moved to bio->bi_iter.bi_sector
	// TODO the judge condition and out information

	// bi_iter.bi_size is the number ofremained bi_vec
	sector = bio->bi_iter.bi_sector;
	capacity = get_capacity(bio->bi_disk);
	if (sector + (bio->bi_iter.bi_size >> SECTOR_SHIFT) > capacity)
		goto out;

	// Get the request vector
	// bio_rw and READA has been removed
	// https://patchwork.kernel.org/patch/9173331/
	// bio_data_dir == (op_is_write(bio_op(bio)) ? WRITE : READ) 1 0
	/*rw = bio_rw(bio);
	if (rw == READA)
		rw = READ;*/
	rw = bio_data_dir(bio);

	// Perform each part of a request
	bio_for_each_segment(bvec, bio, iter)
	{
		unsigned int len = bvec.bv_len;
		// Every biovec means a SEGMENT of a PAGE
		err = nvm_do_bvec(nvm_dev, bvec.bv_page, len, bvec.bv_offset, rw, sector);
		if (err)
		{
			printk(KERN_WARNING "Transfer data in Segments %x of address %x failed\n", len, bvec.bv_page);
			break;
		}
		sector += len >> SECTOR_SHIFT;
		//TODO BUT MAY BUG secotr not update
	}
out:
	bio_endio(bio);
	return;
}

/**
 * Process a single request
 */
static int nvm_do_bvec(struct nvm_device *device, struct page *page,
					   unsigned int len, unsigned int off, int rw, sector_t sector)
{
	void *mem;
	int err = 0;

	mem = kmap_atomic(page);
	if (rw == READ)
	{
		copy_from_nvm(mem + off, device, sector, len);
		flush_dcache_page(page); // if D-cache aliasing is not an issue
	}
	else
	{
		flush_dcache_page(page);
		copy_to_nvm(device, mem + off, sector, len);
	}
	kunmap_atomic(mem);

	return err;
}

/**
 * Copy n bytes to from the NVM to dest starting at the given sector
 */
void __always_inline copy_from_nvm(void *dest, struct nvm_device *device,
								   sector_t sector, size_t n)
{
	const void *nvm;
	nvm = device->nvmdev_data + (sector << SECTOR_SHIFT);
	memory_copy(dest, nvm, n);
}

void __always_inline copy_to_nvm(struct nvm_device *device,
								 const void *src, sector_t sector, size_t n)
{
	void *nvm;
	nvm = device->nvmdev_data + (sector << SECTOR_SHIFT);
	memory_copy(nvm, src, n);
}

/**
 * Perform I/O control
 */
static int nvm_ioctl(struct block_device *bdev, fmode_t mode,
					 unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static int nvm_disk_getgeo(struct block_device *bdev,
						   struct hd_geometry *geo)
{

	long size;
	struct nvm_device *dev = bdev->bd_disk->private_data;

	size = dev->nvmdev_capacity * (HARDSECT_SIZE / KERNEL_SECT_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 4;
	return 0;
}

/**
 * The driver function for NVM Block Drivers
 * 
 * Contains :
 *          1. nvm_init()
 *          2. nvm_exit()
 * */

static int __init nvm_init(void)
{
	int i;
	struct nvm_device *device, *next;

	g_highmem_size = (u64)(nvm_capacity_mb) << MB_PER_BYTES_SHIFT;
	printk(KERN_ERR "NVMSIM:%llu %llu %llu %llu\n",
		   g_highmem_size, g_highmem_phys_addr, nvm_capacity_mb, LLONG_MAX);

	// remap the highmem physical address
	if (NVM_USE_HIGHMEM())
	{
		if (nvm_highmem_map() == NULL)
			return -ENOMEM;
	}

	// register a block device number
	if (register_blkdev(NVM_MAJOR, NVM_DEVICES_NAME) != 0)
	{
		printk(KERN_INFO "The device major number %d is occupied\n", NVM_MAJOR);
		return -EIO;
	}

	// allocate block device and gendisk
	for (i = 0; i < nvm_num_devices; i++)
	{
		device = nvm_alloc(i, nvm_capacity_mb);
		if (!device)
			goto out_free;
		// initialize a request queue
		list_add_tail(&device->nvmdev_list, &nvm_list_head);
	}

	// Register block devices's gendisk
	list_for_each_entry(device, &nvm_list_head, nvmdev_list)
	{
		add_disk(device->nvmdev_disk);
	}
	printk(KERN_INFO "nvm: module loaded\n");
	return 0;

out_free:
	list_for_each_entry_safe(device, next, &nvm_list_head, nvmdev_list)
	{
		list_del(&device->nvmdev_list);
		nvm_free(device);
	}
	unregister_blkdev(NVM_MAJOR, NVM_DEVICES_NAME);
	return -ENOMEM;
}

/**
 * Delete a device
 */
static void nvm_del_one(struct nvm_device *device)
{
	list_del(&device->nvmdev_list);
	del_gendisk(device->nvmdev_disk);
	nvm_free(device);
}

/**
 * Deinitalize a module
 */
static void __exit nvm_exit(void)
{
	unsigned long range;
	struct nvm_device *nvmsim, *next;

	range = nvm_num_devices ? nvm_num_devices : 1UL << (MINORBITS - 1);

	list_for_each_entry_safe(nvmsim, next, &nvm_list_head, nvmdev_list)
	{
		nvm_del_one(nvmsim);
	}

	blk_unregister_region(MKDEV(NVM_MAJOR, 0), range);
	unregister_blkdev(NVM_MAJOR, NVM_DEVICES_NAME);
}

/**
 * NVM Module declarations
 */
module_init(nvm_init);
module_exit(nvm_exit);
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(NVM_MAJOR);