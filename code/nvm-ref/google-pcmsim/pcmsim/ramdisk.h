/*
 * ramdisk.h
 * PCM Simulator: RAM backed block device driver
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

#ifndef __PCMSIM_RAMDISK_H
#define __PCMSIM_RAMDISK_H

#include "pcm.h"

#define PCMSIM_MAJOR		231
#define SECTOR_SHIFT		9
#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		(1 << PAGE_SECTORS_SHIFT)


/**
 * The simulated PCM device with a RAM disk backing store
 */
struct pcmsim_device {
	
	/// The device number
	int pcmsim_number;

	/// The capacity in sectors
	unsigned pcmsim_capacity;

	/// The backing data store
	void* pcmsim_data;

	/// The lock protecting the data store
	spinlock_t pcmsim_lock;

	/// Request queue
	struct request_queue* pcmsim_queue;

	/// Disk
	struct gendisk* pcmsim_disk;

	/// The collection of lists the device belongs to
	struct list_head pcmsim_list;

	/// Additional PCM model data
	struct pcm_model* pcmsim_model;
};


/**
 * Allocate the PCM device
 */
struct pcmsim_device* pcmsim_alloc(int index, unsigned capacity_mb);

/**
 * Free a PCM device
 */
void pcmsim_free(struct pcmsim_device *pcmsim);

#endif
