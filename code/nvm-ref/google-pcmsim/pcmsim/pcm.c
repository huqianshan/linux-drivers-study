/*
 * pcm.c
 * PCM Simulator: PCM Model
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
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/timex.h>

#include <asm/uaccess.h>

#include "memory.h"
#include "pcm.h"
#include "ramdisk.h"
#include "util.h"


/**
 * The original PCM timings
 */
unsigned pcm_org_tRCD = 22;
unsigned pcm_org_tRP  = 60;

/**
 * The original PCM frequency
 */
unsigned pcm_org_mhz  = 400;

/**
 * The extrapolated PCM timings
 */
unsigned pcm_tRCD;
unsigned pcm_tRP;

/**
 * PCM logical row width (bytes)
 */
unsigned pcm_row_width = 256;

/**
 * The PCM latency for reading and writing
 */
unsigned pcm_latency[2 /* 0 = read, 1 = write */][PCMSIM_MEM_SECTORS + 1];

/**
 * The PCM delta latency for reading and writing
 */
int pcm_latency_delta[2 /* 0 = read, 1 = write */][PCMSIM_MEM_SECTORS + 1];


/**
 * Calibrate the PCM model. This function can be called only after
 * the memory subsystem has been initialized.
 */
void pcm_calibrate(void)
{
	unsigned sectors, n;
	unsigned mem_rows, pcm_rows, mem_t;
	unsigned d_read, d_write;

	WARN_ON(sizeof(unsigned) != 4);

	
	// Extrapolate PCM timing information to the current bus frequency

	pcm_tRCD = 10 * pcm_org_tRCD * memory_bus_mhz / pcm_org_mhz;
	pcm_tRP  = 10 * pcm_org_tRP  * memory_bus_mhz / pcm_org_mhz;

	if (pcm_tRCD % 10 >= 5) pcm_tRCD += 10;
	if (pcm_tRP  % 10 >= 5) pcm_tRP  += 10;
	pcm_tRCD /= 10;
	pcm_tRP  /= 10;

	
	// Compute the PCM latencies

	for (sectors = 1; sectors <= PCMSIM_MEM_SECTORS; sectors++) {

		mem_rows = (sectors << 9) / memory_row_width;
		pcm_rows = (sectors << 9) /    pcm_row_width;

		mem_t    = memory_overhead_read[PCMSIM_MEM_UNCACHED][sectors];
		d_read   = pcm_rows * pcm_tRCD - mem_rows * memory_tRCD;
		d_write  = pcm_rows * pcm_tRP  - mem_rows * memory_tRP ;

		pcm_latency[PCM_READ ][sectors] = mem_t + d_read  * memory_bus_scale;
		pcm_latency[PCM_WRITE][sectors] = mem_t + d_write * memory_bus_scale;
	}


	// Compute the deltas

	for (sectors = 1; sectors <= PCMSIM_MEM_SECTORS; sectors++) {

		mem_t    = memory_overhead_read[PCMSIM_MEM_UNCACHED][sectors];

		pcm_latency_delta[PCM_READ ][sectors] = (int) pcm_latency[PCM_READ ][sectors] - (int) mem_t;
		pcm_latency_delta[PCM_WRITE][sectors] = (int) pcm_latency[PCM_WRITE][sectors] - (int) mem_t;
	}

	
	// Print a report

	printk("\n");
	printk("  PCMSIM PCM Settings  \n");
	printk("-----------------------\n");
	printk("\n");
	printk("tRCD          : %4d bus cycles\n", pcm_tRCD);
	printk("tRP           : %4d bus cycles\n", pcm_tRP);
	printk("\n");
	printk("pcm\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s  : %5d cycles read, %6d cycles write\n",
			   n, n == 1 ? " " : "s", pcm_latency[PCM_READ ][n], pcm_latency[PCM_WRITE][n]);
	}
	printk("\n");
	printk("pcm delta\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s  : %5d cycles read, %6d cycles write\n",
			   n, n == 1 ? " " : "s", pcm_latency_delta[PCM_READ ][n], pcm_latency_delta[PCM_WRITE][n]);
	}
	printk("\n");
}


/**
 * Allocate PCM model data
 */
struct pcm_model* pcm_model_allocate(unsigned sectors)
{
	struct pcm_model* model;


	// Allocate the model struct
	
	model = (struct pcm_model*) kzalloc(sizeof(struct pcm_model), GFP_KERNEL);
	if (model == NULL) goto out;


	// Allocate the dirty bits array
	
	model->dirty = (unsigned*) kzalloc(sectors / (sizeof(unsigned) << 3) + sizeof(unsigned), GFP_KERNEL);
	if (model->dirty == NULL) goto out_free;

	return model;


	// Cleanup on error

out_free:
	kfree(model);
out:
	return NULL;
}


/**
 * Free PCM model data
 */
void pcm_model_free(struct pcm_model* model)
{
	unsigned total_reads;
	unsigned total_writes;
	unsigned cached_reads;
	unsigned cached_writes;


	// Compute some statistics

	total_reads  = model->stat_reads [0] + model->stat_reads [1];
	total_writes = model->stat_writes[0] + model->stat_writes[1];

	cached_reads = 0;
	cached_writes = 0;

	if (total_reads > 0) {
		cached_reads = (10000 * model->stat_reads[1]) / total_reads;
	}

	if (total_writes > 0) {
		cached_writes = (10000 * model->stat_writes[1]) / total_writes;
	}


	// Print the statistics

	printk("\n");
	printk("  PCMSIM Statistics  \n");
	printk("---------------------\n");
	printk("\n");
	printk("Reads         : %6d (%2d.%02d%% cached)\n", total_reads , cached_reads  / 100, cached_reads  % 100);
	printk("Writes        : %6d (%2d.%02d%% cached)\n", total_writes, cached_writes / 100, cached_writes % 100);
	printk("\n");
	

	// Free the data structures
	
	kfree(model->dirty);
	kfree(model);
}


/**
 * Perform a PCM read access
 */
void pcm_read(struct pcm_model* model, void* dest, const void* src, size_t length, sector_t sector)
{
	unsigned T, before, after;
	unsigned sectors;
#ifndef PCMSIM_IGNORE_L2
	int cached;
#endif
#ifndef PCMSIM_GROUND_TRUTH
	unsigned t;
#endif

	sectors = length >> SECTOR_SHIFT;
	WARN_ON(sectors > PCMSIM_MEM_SECTORS);


	// Get the ground truth

#ifdef PCMSIM_GROUND_TRUTH
	cached = memory_was_cached(src, length);
#endif


	// Perform the operation

	before = rdtsc();
	memory_copy(dest, src, length);		// This does mfence, so we do not need pipeline flush
	after = rdtsc();
	T = after - before;


	// Handle L2 effects

#ifdef PCMSIM_IGNORE_L2
	model->budget += pcm_latency_delta[PCM_READ][sectors];
#else


	// Classify the time

#ifndef PCMSIM_GROUND_TRUTH
	cached = T < memory_time_l2_threshold_copy[sectors];
	if (!cached) {
		cached = T > memory_time_l2_threshold_copy_cb_lo[sectors]
		      && T < memory_time_l2_threshold_copy_cb_hi[sectors];
	}
#endif

	model->stat_reads[cached]++;


	// Uncached reads

	if (!cached) {

		// Time budget

		model->budget += pcm_latency_delta[PCM_READ][sectors];


		// Clear the dirty bit

		model->dirty[sector >> (UNSIGNED_SHIFT + 3)] &= ~(1 << (sector & 0x1f));
	}
#endif


	// Stall

#ifndef PCMSIM_GROUND_TRUTH
	t = rdtsc();
	model->budget -= (int) (t - after);
	while (model->budget >= (int) overhead_get_ticks) {
		T = rdtsc();
		model->budget -= (int)(T - t);
		t = T;
	}
#endif
}


/**
 * Perform a PCM write access
 */
void pcm_write(struct pcm_model* model, void* dest, const void* src, size_t length, sector_t sector)
{
	unsigned T, before, after;
	unsigned sectors;
#ifndef PCMSIM_IGNORE_L2
	int cached, dirty;
#endif
#ifndef PCMSIM_GROUND_TRUTH
	unsigned t;
#endif

	sectors = length >> SECTOR_SHIFT;
	WARN_ON(sectors > PCMSIM_MEM_SECTORS);


	// Get the ground truth

#ifdef PCMSIM_GROUND_TRUTH
	cached = memory_was_cached(dest, length);
#endif


	// Perform the operation

	before = rdtsc();
	memory_copy(dest, src, length);		// This does mfence, so we do not need pipeline flush
	after = rdtsc();
	T = after - before;


	// Handle L2 effects

#ifdef PCMSIM_IGNORE_L2
	model->budget += pcm_latency_delta[PCM_WRITE][sectors];
#else


	// Classify the time

#ifndef PCMSIM_GROUND_TRUTH
	if (T < memory_time_l2_threshold_copy[sectors]) {
		cached = T < memory_time_l2_threshold_copy_write[1][sectors];
	}
	else {
		cached = (T > memory_time_l2_threshold_copy_write_lo[sectors]
		       && T < memory_time_l2_threshold_copy_write[0][sectors]);
	}
#endif

	model->stat_writes[cached]++;


	// Get the dirty bit

	dirty = (model->dirty[sector >> (UNSIGNED_SHIFT + 3)] & (1 << (sector & 0x1f))) != 0;


	// Set the dirty bit
	
	model->dirty[sector >> (UNSIGNED_SHIFT + 3)] |= 1 << (sector & 0x1f);


	// Time

	if (!(cached && dirty)) {
		model->budget += pcm_latency_delta[PCM_WRITE][sectors];
	}
#endif


	// Stall

#ifndef PCMSIM_GROUND_TRUTH
	t = rdtsc();
	model->budget -= (int) (t - after);
	while (model->budget >= (int) overhead_get_ticks) {
		T = rdtsc();
		model->budget -= (int)(T - t);
		t = T;
	}
#endif
}
