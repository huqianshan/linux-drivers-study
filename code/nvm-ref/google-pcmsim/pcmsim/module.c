/*
 * module.c
 * PCM Simulator: Main code
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
#include <linux/vmalloc.h>
#include <linux/buffer_head.h>

#include <asm/uaccess.h>

#include "memory.h"
#include "ramdisk.h"
#include "pcm.h"
#include "util.h"


/**
 * The maximum number of PCM devices
 */
static int pcmsim_num_devices = 1;

module_param(pcmsim_num_devices, int, 0);
MODULE_PARM_DESC(rd_nr, "Maximum number of simulated PCM devices");

/**
 * Size of each PCM disk in MB
 */
#ifndef __LP64__
static int pcmsim_capacity_mb = 128;
#else
static int pcmsim_capacity_mb = 1024;
#endif

module_param(pcmsim_capacity_mb, int, 0);
MODULE_PARM_DESC(pcmsim_capacity_mb, "Size of each PCM disk in MB");

/**
 * The list of PCM devices
 */
static LIST_HEAD(pcmsim_devices);

/**
 * The mutex guarding the list of devices
 */
static DEFINE_MUTEX(pcmsim_devices_mutex);


/**
 * Initialize one device
 */
static struct pcmsim_device *pcmsim_init_one(int i)
{
	struct pcmsim_device *pcmsim;

	list_for_each_entry(pcmsim, &pcmsim_devices, pcmsim_list) {
		if (pcmsim->pcmsim_number == i)
			goto out;
	}

	pcmsim = pcmsim_alloc(i, pcmsim_capacity_mb);
	if (pcmsim) {
		add_disk(pcmsim->pcmsim_disk);
		list_add_tail(&pcmsim->pcmsim_list, &pcmsim_devices);
	}
out:
	return pcmsim;
}


/**
 * Delete a device
 */
static void pcmsim_del_one(struct pcmsim_device *pcmsim)
{
	list_del(&pcmsim->pcmsim_list);
	del_gendisk(pcmsim->pcmsim_disk);
	pcmsim_free(pcmsim);
}


/**
 * Probe a device
 */
static struct kobject *pcmsim_probe(dev_t dev, int *part, void *data)
{
	struct pcmsim_device *pcmsim;
	struct kobject *kobj;

	mutex_lock(&pcmsim_devices_mutex);
	pcmsim = pcmsim_init_one(dev & MINORMASK);
	kobj = pcmsim ? get_disk(pcmsim->pcmsim_disk) : ERR_PTR(-ENOMEM);
	mutex_unlock(&pcmsim_devices_mutex);

	*part = 0;
	return kobj;
}


/**
 * Initialize a module
 */
static int __init pcmsim_init(void)
{
	int i;
	unsigned long range;
	struct pcmsim_device *pcmsim, *next;

	// Initialize the subsystems
	
	util_calibrate();
	memory_calibrate();
	pcm_calibrate();
	

	// Initialize the devices

	if (register_blkdev(PCMSIM_MAJOR, "pcmsim")) return -EIO;

	for (i = 0; i < pcmsim_num_devices; i++) {
		pcmsim = pcmsim_alloc(i, pcmsim_capacity_mb);
		if (!pcmsim) goto out_free;
		list_add_tail(&pcmsim->pcmsim_list, &pcmsim_devices);
	}
	

	// Register the block devices

	list_for_each_entry(pcmsim, &pcmsim_devices, pcmsim_list) {
		add_disk(pcmsim->pcmsim_disk);
	}

	range = pcmsim_num_devices ? pcmsim_num_devices : 1UL << (MINORBITS - 1);
	blk_register_region(MKDEV(PCMSIM_MAJOR, 0), range, THIS_MODULE, pcmsim_probe, NULL, NULL);


	// Finalize

	printk(KERN_INFO "pcmsim: module loaded\n");
	return 0;


	// Cleanup on error: Free all devices

out_free:
	list_for_each_entry_safe(pcmsim, next, &pcmsim_devices, pcmsim_list) {
		list_del(&pcmsim->pcmsim_list);
		pcmsim_free(pcmsim);
	}
	unregister_blkdev(PCMSIM_MAJOR, "pcmsim");

	return -ENOMEM;
}


/**
 * Deinitalize a module
 */
static void __exit pcmsim_exit(void)
{
	unsigned long range;
	struct pcmsim_device *pcmsim, *next;

	range = pcmsim_num_devices ? pcmsim_num_devices : 1UL << (MINORBITS - 1);

	list_for_each_entry_safe(pcmsim, next, &pcmsim_devices, pcmsim_list) {
		pcmsim_del_one(pcmsim);
	}

	blk_unregister_region(MKDEV(PCMSIM_MAJOR, 0), range);
	unregister_blkdev(PCMSIM_MAJOR, "pcmsim");
}


/**
 * PCM Module declarations
 */
module_init(pcmsim_init);
module_exit(pcmsim_exit);
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(PCMSIM_MAJOR);
MODULE_ALIAS("pcm");
