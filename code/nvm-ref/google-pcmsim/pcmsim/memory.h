/*
 * memory.h
 * PCM Simulator: Memory hierarchy code
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

#ifndef __PCMSIM_MEMORY_H
#define __PCMSIM_MEMORY_H

#include "config.h"

/**
 * Memory cache codes
 */
#define PCMSIM_MEM_UNCACHED	0
#define PCMSIM_MEM_CACHED	1


#ifndef __PCMSIM_MEM_NO_EXTERN

/**
 * The overhead of get_ticks()
 */
extern unsigned memory_overhead_get_ticks;

/**
 * The threshold per number of sectors below which we can assume cached reads
 */
extern unsigned memory_time_l2_threshold_copy[PCMSIM_MEM_SECTORS + 1];

/**
 * The threshold per number of sectors above which we consider memory_time_l2_threshold_copy_write[0][?]
 */
extern unsigned memory_time_l2_threshold_copy_write_lo[PCMSIM_MEM_SECTORS + 1];

/**
 * The threshold per number of sectors below which we can assume cached writes
 */
extern unsigned memory_time_l2_threshold_copy_write[2 /* 0 = uncached read */][PCMSIM_MEM_SECTORS + 1];

/**
 * The average overhead of memory_was_cached() per number of sectors
 */
extern unsigned memory_overhead_was_cached[2 /* 0 = uncached, 1 = cached */][PCMSIM_MEM_SECTORS + 1];

/**
 * The average overhead of memory_read() per number of sectors
 */
extern unsigned memory_overhead_read[2 /* 0 = uncached, 1 = cached */][PCMSIM_MEM_SECTORS + 1];

/**
 * The threshold per number of sectors for cached reads and uncached writes + write-back
 */
extern unsigned memory_time_l2_threshold_copy_cb_lo[PCMSIM_MEM_SECTORS + 1];
extern unsigned memory_time_l2_threshold_copy_cb_hi[PCMSIM_MEM_SECTORS + 1];

/**
 * Memory bus speed
 */
extern unsigned memory_bus_mhz;

/**
 * Memory bus scaling factor
 */
extern unsigned memory_bus_scale;

/**
 * Logical memory row width (bytes per row-to-row advance)
 */
extern unsigned memory_row_width;

/**
 * Memory timing information
 */
extern unsigned memory_tRCD;
extern unsigned memory_tRP;

#endif


/**
 * Calibrate the timer to determine whether there was an L2 cache miss or not
 */
void memory_calibrate(void);

/**
 * Return the current value of the processor's tick counter
 */
u64 get_ticks(void);

/**
 * Determine whether the given buffer was present in its entirety
 * in the L2 cache before this function has been called. This function
 * loads the buffer to the cache as a part of its function, and in order
 * to function properly, it assumes that the buffer offset and the size
 * are aligned to a cache-line size.
 */
int memory_was_cached(const void* buffer, size_t size);

/**
 * Copy a memory buffer
 */
void memory_copy(void* dest, const void* buffer, size_t size);

#endif
