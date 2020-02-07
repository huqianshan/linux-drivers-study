/*
 * ramdisk.c
 * PCM Simulator: Ram backed block device driver
 *
 * Parts derived from drivers/block/brd.c, drivers/block/rd.c,
 * and drivers/block/loop.c, copyright of their respective owners.
 *
 * Copyright 2010
 *      The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/radix-tree.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/buffer_head.h>

#include <asm/uaccess.h>

#include "config.h"
#include "ramdisk.h"
#include "memory.h"
#include "pcm.h"
#include "util.h"


/**
 * Copy n bytes from src to the PCM starting at the given sector
 */
void __always_inline copy_to_pcmsim(struct pcmsim_device *pcmsim, const void *src,
			                        sector_t sector, size_t n)
{
	void *pcm;
#ifndef PCMSIM_RAMDISK_ONLY
	unsigned l, o;
#endif

	pcm = pcmsim->pcmsim_data + (sector << SECTOR_SHIFT);

#ifndef PCMSIM_RAMDISK_ONLY
	o = 0;
	while (n > 0) {
		l = n;
		if (l > PCMSIM_MEM_SECTORS << SECTOR_SHIFT) l = PCMSIM_MEM_SECTORS << SECTOR_SHIFT;
		pcm_write(pcmsim->pcmsim_model, ((char*) pcm) + o, ((const char*) src) + o, l, sector);
		n -= l;
	}
#else
	memory_copy(pcm, src, n);
#endif
}


/**
 * Copy n bytes to from the PCM to dest starting at the given sector
 */
void __always_inline copy_from_pcmsim(void *dest, struct pcmsim_device *pcmsim,
									  sector_t sector, size_t n)
{
	const void *pcm;
#ifndef PCMSIM_RAMDISK_ONLY
	unsigned l, o;
#endif

	pcm = pcmsim->pcmsim_data + (sector << SECTOR_SHIFT);

#ifndef PCMSIM_RAMDISK_ONLY
	o = 0;
	while (n > 0) {
		l = n;
		if (l > PCMSIM_MEM_SECTORS << SECTOR_SHIFT) l = PCMSIM_MEM_SECTORS << SECTOR_SHIFT;
		pcm_read(pcmsim->pcmsim_model, ((char*) dest) + o, ((const char*) pcm) + o, l, sector);
		n -= l;
	}
#else
	memory_copy(dest, pcm, n);
#endif
}


/**
 * Process a single request
 */
static int pcmsim_do_bvec(struct pcmsim_device *pcmsim, struct page *page,
                          unsigned int len, unsigned int off, int rw, sector_t sector)
{
	void *mem;
	int err = 0;

	mem = kmap_atomic(page, KM_USER0);
	if (rw == READ) {
		copy_from_pcmsim(mem + off, pcmsim, sector, len);
		flush_dcache_page(page);
	}
	else {
		flush_dcache_page(page);
		copy_to_pcmsim(pcmsim, mem + off, sector, len);
	}
	kunmap_atomic(mem, KM_USER0);

	return err;
}


/**
 * Process pending requests from the queue
 */
static int pcmsim_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct pcmsim_device *pcmsim = bdev->bd_disk->private_data;
	int rw;
	struct bio_vec *bvec;
	sector_t sector;
	int i;
	int err = -EIO;
	unsigned capacity;
	

	// Check the device capacity

	sector = bio->bi_sector;
	capacity = get_capacity(bdev->bd_disk);
	if (sector + (bio->bi_size >> SECTOR_SHIFT) > capacity) goto out;


	// Get the request vector

	rw = bio_rw(bio);
	if (rw == READA) rw = READ;


	// Perform each part of a request

	bio_for_each_segment(bvec, bio, i) {
		unsigned int len = bvec->bv_len;
		err = pcmsim_do_bvec(pcmsim, bvec->bv_page, len, bvec->bv_offset, rw, sector);
		if (err) break;
		sector += len >> SECTOR_SHIFT;
	}


	// Cleanup

out:
	bio_endio(bio, err);

	return 0;
}


/**
 * Perform I/O control
 */
static int pcmsim_ioctl(struct block_device *bdev, fmode_t mode,
					    unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}


/**
 * PCM block device operations
 */
static struct block_device_operations pcmsim_fops = {
	.owner        = THIS_MODULE,
	.locked_ioctl = pcmsim_ioctl,
};


/**
 * Allocate the PCM device
 */
struct pcmsim_device* pcmsim_alloc(int index, unsigned capacity_mb)
{
	struct pcmsim_device *pcmsim;
	struct gendisk *disk;


	// Allocate the device

	pcmsim = kzalloc(sizeof(*pcmsim), GFP_KERNEL);
	if (!pcmsim) goto out;
	pcmsim->pcmsim_number = index;
	pcmsim->pcmsim_capacity = capacity_mb * 1024 * 2;
	spin_lock_init(&pcmsim->pcmsim_lock);


	// Allocate the PCM model

	pcmsim->pcmsim_model = pcm_model_allocate(pcmsim->pcmsim_capacity);
	if (!pcmsim->pcmsim_model) goto out_free_struct;


	// Allocate the backing store
	
	pcmsim->pcmsim_data = vmalloc(capacity_mb * 1048576);
	if (!pcmsim->pcmsim_data) goto out_free_model;


	// Allocate the block request queue

	pcmsim->pcmsim_queue = blk_alloc_queue(GFP_KERNEL);
	if (!pcmsim->pcmsim_queue) goto out_free_dev;
	blk_queue_make_request(pcmsim->pcmsim_queue, pcmsim_make_request);
	blk_queue_ordered     (pcmsim->pcmsim_queue, QUEUE_ORDERED_TAG, NULL);
	/*blk_queue_max_sectors (pcmsim->pcmsim_queue, 1024);*/
	blk_queue_bounce_limit(pcmsim->pcmsim_queue, BLK_BOUNCE_ANY);


	// Allocate the disk device

	disk = pcmsim->pcmsim_disk = alloc_disk(1 /* cannot be partitioned */);
	if (!disk) goto out_free_queue;
	disk->major	       = PCMSIM_MAJOR;
	disk->first_minor  = index;
	disk->fops         = &pcmsim_fops;
	disk->private_data = pcmsim;
	disk->queue	       = pcmsim->pcmsim_queue;
	disk->flags       |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "pcm%d", index);
	set_capacity(disk, capacity_mb * 1024 * 2);

	return pcmsim;


	// Cleanup on error

out_free_queue:
	blk_cleanup_queue(pcmsim->pcmsim_queue);
out_free_dev:
	vfree(pcmsim->pcmsim_data);
out_free_model:
	pcm_model_free(pcmsim->pcmsim_model);
out_free_struct:
	kfree(pcmsim);
out:
	return NULL;
}


/**
 * Free a PCM device
 */
void pcmsim_free(struct pcmsim_device *pcmsim)
{
	put_disk(pcmsim->pcmsim_disk);
	blk_cleanup_queue(pcmsim->pcmsim_queue);
	pcm_model_free(pcmsim->pcmsim_model);
	if (pcmsim->pcmsim_data != NULL) vfree(pcmsim->pcmsim_data);
	kfree(pcmsim);
}
