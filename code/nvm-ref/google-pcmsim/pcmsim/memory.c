/*
 * memory.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/timex.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>

#define __PCMSIM_MEM_NO_EXTERN
#include "memory.h"
#include "util.h"


/**
 * The threshold (the number of ticks), below which we can assume
 * no L2 misses in a result of time_read()
 */
unsigned memory_time_l2_threshold = 0;

/**
 * The threshold per number of sectors below which we can assume cached reads
 */
unsigned memory_time_l2_threshold_copy[PCMSIM_MEM_SECTORS + 1];

/**
 * The threshold per number of sectors below which we can assume cached writes
 */
unsigned memory_time_l2_threshold_copy_write[2 /* 0 = uncached read */][PCMSIM_MEM_SECTORS + 1];

/**
 * The threshold per number of sectors above which we consider memory_time_l2_threshold_copy_write[0][?]
 */
unsigned memory_time_l2_threshold_copy_write_lo[PCMSIM_MEM_SECTORS + 1];

/**
 * The threshold per number of sectors for cached reads and uncached writes + write-back
 */
unsigned memory_time_l2_threshold_copy_cb_lo[PCMSIM_MEM_SECTORS + 1];
unsigned memory_time_l2_threshold_copy_cb_hi[PCMSIM_MEM_SECTORS + 1];

/**
 * The average overhead of memory_was_cached() per number of sectors
 */
unsigned memory_overhead_was_cached[2 /* 0 = uncached, 1 = cached */][PCMSIM_MEM_SECTORS + 1];

/**
 * The average overhead of memory_read() per number of sectors
 */
unsigned memory_overhead_read[2 /* 0 = uncached, 1 = cached */][PCMSIM_MEM_SECTORS + 1];

/**
 * The average overhead of memory_copy() per number of sectors
 */
unsigned memory_overhead_copy[3 /* from */][3 /* to, 2 = uncached+wb */][PCMSIM_MEM_SECTORS + 1];

/**
 * The variance of overhead of memory_read() per number of sectors
 */
unsigned memory_var_overhead_read[2 /* 0 = uncached, 1 = cached */][PCMSIM_MEM_SECTORS + 1];

/**
 * The variance of overhead of memory_copy() per number of sectors
 */
unsigned memory_var_overhead_copy[3 /* from */][3 /* to, 2 = uncached+wb */][PCMSIM_MEM_SECTORS + 1];

/**
 * The accuracy of overhead of memory_copy() per number of sectors
 */
unsigned memory_okr_overhead_copy[3 /* from */][3 /* to, 2 = uncached+wb */][PCMSIM_MEM_SECTORS + 1];
unsigned memory_okw_overhead_copy[3 /* from */][3 /* to, 2 = uncached+wb */][PCMSIM_MEM_SECTORS + 1];

/**
 * DDR version
 */
unsigned memory_ddr_version = 1;

/**
 * DDR rating
 */
unsigned memory_ddr_rating = 333;

/**
 * Memory bus speed
 */
unsigned memory_bus_mhz;

/**
 * Memory bus scaling factor
 */
unsigned memory_bus_scale;

/**
 * Logical memory row width (bytes per row-to-row advance)
 */
unsigned memory_row_width = 128;

/**
 * Memory timing information
 */
unsigned memory_tRCD   =  3;
unsigned memory_tRP    =  3;
unsigned memory_tCL10  = 25;	/* x 10 */


/**
 * Flush the pipeline
 */
static __inline__ void flush(void)
{
#ifndef __LP64__

	asm("pushl %eax\n\t"
		"pushl %ebx\n\t"
		"pushl %ecx\n\t"
		"pushl %edx\n\t"
		"xorl %eax, %eax\n\t"
		"cpuid\n\t"
		"popl %edx\n\t"
		"popl %ecx\n\t"
		"popl %ebx\n\t"
		"popl %eax\n\t");

#else

	asm("pushq %rax\n\t"
		"pushq %rbx\n\t"
		"pushq %rcx\n\t"
		"pushq %rdx\n\t"
		"xorq %rax, %rax\n\t"
		"cpuid\n\t"
		"popq %rdx\n\t"
		"popq %rcx\n\t"
		"popq %rbx\n\t"
		"popq %rax\n\t");

#endif
}


/**
 * Measure the maximum time it takes to read a double-word
 * from each cache line of the given buffer. Return the number
 * of ticks, including the overhead of measurement.
 */
static unsigned noinline memory_time_read(const void* buffer, size_t size)
{
	unsigned t;

#ifndef __LP64__

	// Take the time measurement every 64 bytes (a typical length
	// of Pentium's L2 cache line)

	// Overhead of cpuid: 400 ticks on Pentium 4, 3 GHz

	asm("pushl %%eax\n\t"
		"pushl %%esi\n\t"
		"pushl %%ebx\n\t"
		"pushl %%ecx\n\t"
		"pushl %%edx\n\t"
		"pushl %%ebp\n\t"

		"cld\n\t"
		"cli\n\t"

#ifdef PCMSIM_MEM_DISABLE_PREFETCH

		// Disable prefetch

		"pushl %%eax\n\t"
		"pushl %%ecx\n\t"
		"movl $0x1a0, %%ecx\n\t"
		"rdmsr\n\t"
		"movl $0x40200, %%ecx\n\t"
		"notl %%ecx\n\t"
		"andl %%ecx, %%eax\n\t"
		"movl $0x1a0, %%ecx\n\t"
		"wrmsr\n\t"
		"popl %%ecx\n\t"
		"popl %%eax\n\t"

#endif

		// Initialize the timer

		"xorl %%eax, %%eax\n\t"
		"movl %%eax, %%ebp\n\t"
		"lfence\n\t"
		"rdtsc\n\t"
		"pushl %%edx\n\t"
		"pushl %%eax\n\t"

		// Main loop

		"l:\n\t"

			"lodsl\n\t"
			"addl $60, %%esi\n\t"

			// Flush the pipeline

			"lfence\n\t"

			// Get the time

			"rdtsc\n\t"
			"movl (%%esp), %%ebx\n\t"
			"movl -4(%%esp),  %%ecx\n\t"
			"movl %%eax, (%%esp)\n\t"
			"movl %%edx, -4(%%esp)\n\t"

			// Keep the maximum interval (in the %ebp register)

			"subl %%ebx, %%eax\n\t"
			"sbbl %%ecx, %%edx\n\t"
			"cmpl %%ebp, %%eax\n\t"
			"cmovgl %%eax, %%ebp\n\t"

			// Repeat as long as %edi > 0

			"decl %%edi\n\t"
			"jnz l\n\t"

		"popl %%ebx\n\t"
		"popl %%ebx\n\t"

#ifdef PCMSIM_MEM_DISABLE_PREFETCH

		// Enable prefetch

		"pushl %%eax\n\t"
		"pushl %%ecx\n\t"
		"movl $0x1a0, %%ecx\n\t"
		"rdmsr\n\t"
		"movl $0x40200, %%ecx\n\t"
		"orl %%ecx, %%eax\n\t"
		"movl $0x1a0, %%ecx\n\t"
		"wrmsr\n\t"
		"popl %%ecx\n\t"
		"popl %%eax\n\t"

#endif

		// Clean-up

		"sti\n\t"

		"movl %%ebp, %%edi\n\t"
		"popl %%ebp\n\t"
		"popl %%edx\n\t"
		"popl %%ecx\n\t"
		"popl %%ebx\n\t"
		"popl %%esi\n\t"
		"popl %%eax\n\t"

		: "=D" (t)
		: "S" (buffer), "D" (size / 64)
	);

#else

	// Take the time measurement every 64 bytes (a typical length
	// of Pentium's L2 cache line)

	// Overhead of cpuid: 200 ticks on Core 2 Duo, 2 GHz

	asm("pushq %%rdi\n\t"
		"pushq %%rsi\n\t"
		"pushq %%rbx\n\t"
		"pushq %%rcx\n\t"
		"pushq %%rdx\n\t"
		"pushq %%rbp\n\t"

		"cld\n\t"

		// Initialize the timer

		"xorq %%rax, %%rax\n\t"
		"movq %%rax, %%rbp\n\t"
		"lfence\n\t"
		"rdtsc\n\t"
		"pushq %%rdx\n\t"
		"pushq %%rax\n\t"

		// Main loop

		"l:\n\t"

			"lodsq\n\t"
			"addq $56, %%rsi\n\t"

			// Flush the pipeline

			"lfence\n\t"

			// Get the time

			"rdtsc\n\t"
			"movq (%%rsp), %%rbx\n\t"
			"movq -8(%%rsp),  %%rcx\n\t"
			"movq %%rax, (%%rsp)\n\t"
			"movq %%rdx, -8(%%rsp)\n\t"

			// Keep the maximum interval (in the %rbp register)

			"subq %%rbx, %%rax\n\t"
			"sbbq %%rcx, %%rdx\n\t"
			"cmpq %%rbp, %%rax\n\t"
			"cmovgq %%rax, %%rbp\n\t"

			// Repeat as long as %rdi > 0

			"decq %%rdi\n\t"
			"jnz l\n\t"

		"popq %%rbx\n\t"
		"popq %%rbx\n\t"

		// Clean-up

		"movq %%rbp, %%rax\n\t"
		"popq %%rbp\n\t"
		"popq %%rdx\n\t"
		"popq %%rcx\n\t"
		"popq %%rbx\n\t"
		"popq %%rsi\n\t"
		"popq %%rdi\n\t"

		: "=a" (t)
		: "S" (buffer), "D" (size / 64)
	);

#endif

	flush();

	return t;
}


/**
 * Read the contents of a buffer
 */
void memory_read(const void* buffer, size_t size)
{

#ifndef __LP64__

	asm("pushl %%eax\n\t"
		"pushl %%esi\n\t"
		"pushl %%ecx\n\t"

		"cld\n\t"
		"rep lodsl\n\t"
		"lfence\n\t"

		"popl %%ecx\n\t"
		"popl %%esi\n\t"
		"popl %%eax\n\t"

		:
		: "S" (buffer), "c" (size >> 2)
	);

#else

	asm("pushq %%rax\n\t"
		"pushq %%rsi\n\t"
		"pushq %%rcx\n\t"

		"cld\n\t"
		"rep lodsq\n\t"
		"lfence\n\t"

		"popq %%rcx\n\t"
		"popq %%rsi\n\t"
		"popq %%rax\n\t"

		:
		: "S" (buffer), "c" (size >> 3)
	);

#endif
}


/**
 * Copy a memory buffer
 */
void memory_copy(void* dest, const void* buffer, size_t size)
{

#ifndef __LP64__

	asm("pushl %%eax\n\t"
		"pushl %%esi\n\t"
		"pushl %%edi\n\t"
		"pushl %%ecx\n\t"

		"cld\n\t"
		"rep movsl\n\t"
		"mfence\n\t"

		"popl %%ecx\n\t"
		"popl %%edi\n\t"
		"popl %%esi\n\t"
		"popl %%eax\n\t"

		:
		: "S" (buffer), "D" (dest), "c" (size >> 2)
	);

#else

	asm("pushq %%rax\n\t"
		"pushq %%rsi\n\t"
		"pushq %%rdi\n\t"
		"pushq %%rcx\n\t"

		"cld\n\t"
		"rep movsq\n\t"
		"mfence\n\t"

		"popq %%rcx\n\t"
		"popq %%rdi\n\t"
		"popq %%rsi\n\t"
		"popq %%rax\n\t"

		:
		: "S" (buffer), "D" (dest), "c" (size >> 3)
	);

#endif
}


/**
 * Compute sample variance
 */
unsigned variance(void** buffers, unsigned* data, unsigned count, unsigned max_count)
{
	unsigned u;
	unsigned mean = 0;
	unsigned ss = 0;

	for (u = 0; u < max_count; u++) {
		if (buffers[u] != NULL) {
			mean += data[u];
		}
	}
	mean /= count;

	for (u = 0; u < max_count; u++) {
		if (buffers[u] != NULL) {
			ss += ((int) data[u] - (int) mean) * ((int) data[u] - (int) mean);
		}
	}

	return ss / (count - 1);
}


/**
 * Compute the half-width of a 95% confidence interval
 */
unsigned hw95(unsigned var, unsigned count)
{
	WARN_ON(count < 90 || count > 100);

	/*
	 * So far use the following:
	 *   t_95,0.975 = 1.985
	 */

	return (1985 * sqrt32(var / count)) / 1000;
}


/**
 * Compute the half-width of a 95% prediction interval
 */
unsigned hw95pi(unsigned var, unsigned count)
{
	WARN_ON(count < 90 || count > 100);

	/*
	 * So far use the following:
	 *   t_95,0.975 = 1.985
	 */

	return (1985 * sqrt32(var + var / count)) / 1000;
}


/**
 * Calibrate the timer to determine whether there was an L2 cache miss or not
 */
void memory_calibrate(void)
{
	unsigned count = 0;
	unsigned max_count = 100;
	void* buffers[max_count];
	void* write_buffer;
	unsigned* dirty_buffer;

	unsigned no_misses = 0;
	unsigned l2_misses = 0;
	unsigned no_misses_total = 0;
	unsigned l2_misses_total = 0;
	unsigned no_misses_ok_read = 0;
	unsigned l2_misses_ok_read = 0;
	unsigned no_misses_ok_write = 0;
	unsigned l2_misses_ok_write = 0;
	
	unsigned wd_times[PCMSIM_MEM_SECTORS + 1];
	unsigned wd_exp = 14;
	unsigned wd_trials = 16;
	unsigned wd_i;
	unsigned wd_temp = 0;
	
	unsigned data_no_misses[max_count];
	unsigned data_l2_misses[max_count];

	unsigned u, s, t, n;
	int d, i, ok;
	int cached;

#ifdef __LP64__
	memory_ddr_version = 2;
	memory_ddr_rating = 667;
	memory_tRCD  = 5;
	memory_tRP   = 5;
	memory_tCL10 = 50;
#endif


	// Initialize

	write_buffer = vmalloc(16 * PCMSIM_MEM_SECTORS * 1024);
	BUG_ON(write_buffer == NULL);

	dirty_buffer = (unsigned*) vmalloc(sizeof(unsigned) * 4 * 1024 * 1024);
	BUG_ON(dirty_buffer == NULL);

	for (u = 0; u < max_count; u++) {
		buffers[u] = vmalloc(16 * PCMSIM_MEM_SECTORS * 1024);
		if (buffers[u] != NULL) count++;
	}


	// Memory bus

	memory_bus_mhz = memory_ddr_rating / 2;
	
	memory_bus_scale = cpu_khz * 10 / (memory_bus_mhz * 1000);
	if (memory_bus_scale % 10 > 5) memory_bus_scale += 10;
	memory_bus_scale /= 10;


	// Measure the latencies for cached and uncached reads

	for (u = 0; u < max_count; u++) {
		if (buffers[u] != NULL) {

			asm volatile("wbinvd");
			s = memory_time_read(buffers[u], 4096);
			t = memory_time_read(buffers[u], 4096);

			if (s > 2 * 2000 || t > 2 * 2000) {
				count--;
				//kfree(buffers[u]);
				vfree(buffers[u]);
				buffers[u] = NULL;
				continue;
			}

			l2_misses += s;
			no_misses += t;
		}
	}

	WARN_ON(count == 0);
	if (count == 0) count++;

	if (l2_misses <= no_misses + count * 4) {
		printk(KERN_WARNING "Could not measure the memory access times\n");
		no_misses = l2_misses / 2;
	}

	memory_time_l2_threshold = (no_misses + (l2_misses - no_misses) / 4) / count;


	// Measure the total cost of memory_was_cached() for a various number of 512 byte sectors

	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {

		l2_misses_total = 0;
		no_misses_total = 0;

		for (u = 0; u < max_count; u++) {
			if (buffers[u] != NULL) {

				asm volatile("wbinvd");

				s = get_ticks();
				memory_was_cached(buffers[u], n << 9);
				s = get_ticks() - s;

				t = get_ticks();
				memory_was_cached(buffers[u], n << 9);
				t = get_ticks() - t;

				s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
				t = t <= overhead_get_ticks ? 0 : t - overhead_get_ticks;

				l2_misses_total += s;
				no_misses_total += t;

				data_l2_misses[u] = s;
				data_no_misses[u] = t;
			}
		}

		memory_overhead_was_cached[0][n] = l2_misses_total / count;
		memory_overhead_was_cached[1][n] = no_misses_total / count;
	}


	//
	// Measure the total cost of memory_read()
	//

	for (i = 0; i <= PCMSIM_MEM_SECTORS; i++) {

		for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					s = get_ticks();
					memory_read(buffers[u], n << 9);
					s = get_ticks() - s;

					asm volatile("mfence");

					t = get_ticks();
					memory_read(buffers[u], n << 9);
					t = get_ticks() - t;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					t = t <= overhead_get_ticks ? 0 : t - overhead_get_ticks;

					l2_misses_total += s;
					no_misses_total += t;

					data_l2_misses[u] = s;
					data_no_misses[u] = t;
				}
			}

			memory_overhead_read[0][n] = l2_misses_total / count;
			memory_overhead_read[1][n] = no_misses_total / count;

			memory_var_overhead_read[0][n] = variance(buffers, data_l2_misses, count, max_count);
			memory_var_overhead_read[1][n] = variance(buffers, data_no_misses, count, max_count);
		}


		// Sanity check

		ok = 1;
		for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
			if (memory_overhead_read[0][n] == 0) ok = 0;
			if (memory_overhead_read[1][n] == 0) ok = 0;
		}
		for (n = 2; n <= PCMSIM_MEM_SECTORS; n++) {
			if (memory_overhead_read[0][n - 1] >= memory_overhead_read[0][n]) ok = 0;
			if (memory_overhead_read[1][n - 1] >= memory_overhead_read[1][n]) ok = 0;
		}
		if (ok) break;
	}


	//
	// Measure the total cost of memory_copy()
	//

	for (i = 0; i <= PCMSIM_MEM_SECTORS; i++) {

		for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {

			// Destination is not cached

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					s = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = get_ticks() - s;

					asm volatile("wbinvd");
					asm volatile("mfence");

					memory_read(buffers[u], n << 9);
					t = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					t = get_ticks() - t;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					t = t <= overhead_get_ticks ? 0 : t - overhead_get_ticks;

					l2_misses_total += s;
					no_misses_total += t;

					data_l2_misses[u] = s;
					data_no_misses[u] = t;
				}
			}

			memory_overhead_copy[0][0][n] = l2_misses_total / count;
			memory_overhead_copy[1][0][n] = no_misses_total / count;

			memory_var_overhead_copy[0][0][n] = variance(buffers, data_l2_misses, count, max_count);
			memory_var_overhead_copy[1][0][n] = variance(buffers, data_no_misses, count, max_count);


			// Destination is not cached + writeback

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					for (s = 0; s < 128 * 1024; s++) dirty_buffer[s] = 0;

					s = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = get_ticks() - s;

					asm volatile("wbinvd");
					asm volatile("mfence");

					for (t = 0; t < 128 * 1024; t++) dirty_buffer[t] = 0;

					memory_read(buffers[u], n << 9);
					t = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					t = get_ticks() - t;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					t = t <= overhead_get_ticks ? 0 : t - overhead_get_ticks;

					l2_misses_total += s;
					no_misses_total += t;

					data_l2_misses[u] = s;
					data_no_misses[u] = t;
				}
			}

			memory_overhead_copy[0][2][n] = l2_misses_total / count;
			memory_overhead_copy[1][2][n] = no_misses_total / count;

			memory_var_overhead_copy[0][2][n] = variance(buffers, data_l2_misses, count, max_count);
			memory_var_overhead_copy[1][2][n] = variance(buffers, data_no_misses, count, max_count);


			// Destination is cached

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					memory_read(write_buffer, n << 9);
					s = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = get_ticks() - s;

					asm volatile("mfence");

					memory_read(write_buffer, n << 9);
					t = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					t = get_ticks() - t;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					t = t <= overhead_get_ticks ? 0 : t - overhead_get_ticks;

					l2_misses_total += s;
					no_misses_total += t;

					data_l2_misses[u] = s;
					data_no_misses[u] = t;
				}
			}

			memory_overhead_copy[0][1][n] = l2_misses_total / count;
			memory_overhead_copy[1][1][n] = no_misses_total / count;

			memory_var_overhead_copy[0][1][n] = variance(buffers, data_l2_misses, count, max_count);
			memory_var_overhead_copy[1][1][n] = variance(buffers, data_no_misses, count, max_count);


			// Source is not cached + writeback

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");
					for (s = 0; s < 128 * 1024; s++) dirty_buffer[s] = 0;

					s = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = get_ticks() - s;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					l2_misses_total += s;

					data_l2_misses[u] = s;
				}
			}

			memory_overhead_copy[2][2][n] = l2_misses_total / count;
			memory_var_overhead_copy[2][2][n] = variance(buffers, data_l2_misses, count, max_count);

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					for (s = 0; s < 128 * 1024; s++) dirty_buffer[s] = 0;
					memory_read(write_buffer, n << 9);

					s = get_ticks();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = get_ticks() - s;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					l2_misses_total += s;

					data_l2_misses[u] = s;
				}
			}

			memory_overhead_copy[2][1][n] = l2_misses_total / count;
			memory_var_overhead_copy[2][1][n] = variance(buffers, data_l2_misses, count, max_count);


			// Threshold - base

			memory_time_l2_threshold_copy[n] = (memory_overhead_copy[0][1][n]
											  + memory_overhead_copy[1][0][n]) / 2;


			// Threshold - read

			memory_time_l2_threshold_copy_cb_lo[n] = (memory_overhead_copy[0][1][n]
											        + memory_overhead_copy[1][2][n]) / 2;
			memory_time_l2_threshold_copy_cb_hi[n] = (memory_overhead_copy[0][0][n]
											        + memory_overhead_copy[1][2][n]) / 2;

			if (memory_overhead_copy[1][2][n] > memory_overhead_copy[0][0][n]) {
				memory_time_l2_threshold_copy_cb_lo[n] = (memory_overhead_copy[0][0][n]
														+ memory_overhead_copy[1][2][n]) / 2;
				memory_time_l2_threshold_copy_cb_hi[n] = 1000000;
			}

			if (memory_overhead_copy[1][2][n] < memory_overhead_copy[0][1][n]) {
				memory_time_l2_threshold_copy_cb_lo[n] = 0;
				memory_time_l2_threshold_copy_cb_hi[n] = (memory_overhead_copy[0][1][n]
				             							+ memory_overhead_copy[1][2][n]) / 2;
			}

			if (memory_overhead_copy[1][2][n] < memory_overhead_copy[2][1][n]
			 && memory_overhead_copy[2][1][n] < memory_overhead_copy[0][1][n]) {
				memory_time_l2_threshold_copy_cb_lo[n] = 0;
				memory_time_l2_threshold_copy_cb_hi[n] = (memory_overhead_copy[2][1][n]
				             							+ memory_overhead_copy[1][2][n]) / 2;
			}


			// Threshold - write

			memory_time_l2_threshold_copy_write[0][n] = (memory_overhead_copy[0][1][n]
											           + memory_overhead_copy[1][2][n]) / 2;
			memory_time_l2_threshold_copy_write[1][n] = (memory_overhead_copy[1][1][n]
											           + memory_overhead_copy[1][0][n]) / 2;

			s = (memory_overhead_copy[0][1][n] + memory_overhead_copy[0][0][n]) / 2;
			if (s > memory_time_l2_threshold_copy_write[0][n]) memory_time_l2_threshold_copy_write[0][n] = s;

			memory_time_l2_threshold_copy_write_lo[n] = memory_time_l2_threshold_copy[n];
			if (memory_overhead_copy[1][2][n] < memory_overhead_copy[0][1][n]) {
				memory_time_l2_threshold_copy_write_lo[n] = (memory_overhead_copy[0][1][n]
				                                           + memory_overhead_copy[1][2][n]) / 2;
			}

			if (memory_overhead_copy[1][2][n] < memory_overhead_copy[2][1][n]
			 && memory_overhead_copy[2][1][n] < memory_overhead_copy[0][1][n]) {
				memory_time_l2_threshold_copy_write_lo[n] = (memory_overhead_copy[2][1][n]
				             							   + memory_overhead_copy[1][2][n]) / 2;
				memory_time_l2_threshold_copy_write[0][n] = (memory_overhead_copy[0][0][n]
												           + memory_overhead_copy[0][1][n]) / 2;
			}
		}


		// Sanity check

		ok = 1;
		for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
			if (memory_overhead_copy[0][0][n] == 0) ok = 0;
			if (memory_overhead_copy[0][1][n] == 0) ok = 0;
			if (memory_overhead_copy[1][0][n] == 0) ok = 0;
			if (memory_overhead_copy[1][1][n] == 0) ok = 0;
		}
		for (n = 2; n <= PCMSIM_MEM_SECTORS; n++) {
			if (memory_overhead_copy[0][0][n - 1] >= memory_overhead_copy[0][0][n]) ok = 0;
			if (memory_overhead_copy[0][1][n - 1] >= memory_overhead_copy[0][1][n]) ok = 0;
			if (memory_overhead_copy[1][0][n - 1] >= memory_overhead_copy[1][0][n]) ok = 0;
			if (memory_overhead_copy[1][1][n - 1] >= memory_overhead_copy[1][1][n]) ok = 0;
		}
		if (ok) break;
	}
	
	
	//
	// Autodetect logical row width (number of bytes per row-to-row advance)
	//

	for (wd_i = 0; wd_i <= wd_trials; wd_i++) {
		for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {

			l2_misses_total = 0;
			no_misses_total = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {
			
					asm volatile("wbinvd");
					asm volatile("lfence");

					s = get_ticks();
					memory_read(buffers[u], n << wd_exp);
					s = get_ticks() - s;

					s = s <= overhead_get_ticks ? 0 : s - overhead_get_ticks;
					l2_misses_total += s;
				}
			}

			wd_times[n] = l2_misses_total / count;
		}
	
		s = 0;
		for (n = 2; n <= PCMSIM_MEM_SECTORS; n++) {
			d = ((int) wd_times[n]) - (int) wd_times[n - 1];
			if (d < 0) d = 0;
			s += d;
		}
		d = s;
		d /= memory_bus_scale * (PCMSIM_MEM_SECTORS - 1);
	
		t  = memory_tRCD + memory_tRP;
		t += memory_tCL10 / 10 + (memory_tCL10 % 10 > 0 ? 1 : 0) - 1;
	
		// s = the number of row-to-row switches
	
		s = 10 * (d - (1 << (wd_exp - 4))) / t;
		if (s % 10 > 0) s += 10;
		s /= 10;
	
		// Round s to the closet power of 2
	
		i = 0;
		t = s;
		while (t > 0) { t >>= 1; i++; }
		s = 1 << (i - (((s >> (i - 2)) & 1) == 0 ? 1 : 0));
		
		wd_temp += (1 << wd_exp) / s;
	}
	
	s = wd_temp / wd_trials;
	i = 0;
	t = s;
	while (t > 0) { t >>= 1; i++; }
	s = 1 << (i - (((s >> (i - 2)) & 1) == 0 ? 1 : 0));
	
	memory_row_width = s;


#ifdef PCMSIM_CHECK_ACCURACY

	//
	// Check prediction accuracy for memory_copy()
	//

	for (i = 0; i <= PCMSIM_MEM_SECTORS; i++) {

		for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {

			// Destination is not cached

			no_misses_ok_read = 0;
			l2_misses_ok_read = 0;
			no_misses_ok_write = 0;
			l2_misses_ok_write = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					s = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = rdtsc() - s;

					asm volatile("wbinvd");
					asm volatile("mfence");

					memory_read(buffers[u], n << 9);
					t = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					t = rdtsc() - t;


					// Read

					cached = s < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = s > memory_time_l2_threshold_copy_cb_lo[n]
						      && s < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if (!cached) l2_misses_ok_read++;

					cached = t < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = t > memory_time_l2_threshold_copy_cb_lo[n]
						      && t < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if ( cached) no_misses_ok_read++;


					// Write

					if (s < memory_time_l2_threshold_copy[n]) {
						cached = s < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (s > memory_time_l2_threshold_copy_write_lo[n]
						       && s < memory_time_l2_threshold_copy_write[0][n]);
					}

					if (!cached) l2_misses_ok_write++;

					if (t < memory_time_l2_threshold_copy[n]) {
						cached = t < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (t > memory_time_l2_threshold_copy_write_lo[n]
						       && t < memory_time_l2_threshold_copy_write[0][n]);
					}

					if (!cached) no_misses_ok_write++;
				}
			}

			memory_okr_overhead_copy[0][0][n] = l2_misses_ok_read;
			memory_okr_overhead_copy[1][0][n] = no_misses_ok_read;
			memory_okw_overhead_copy[0][0][n] = l2_misses_ok_write;
			memory_okw_overhead_copy[1][0][n] = no_misses_ok_write;


			// Destination is not cached + writeback

			no_misses_ok_read = 0;
			l2_misses_ok_read = 0;
			no_misses_ok_write = 0;
			l2_misses_ok_write = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					for (s = 0; s < 128 * 1024; s++) dirty_buffer[s] = 0;

					s = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = rdtsc() - s;

					asm volatile("wbinvd");
					asm volatile("mfence");

					for (t = 0; t < 128 * 1024; t++) dirty_buffer[t] = 0;

					memory_read(buffers[u], n << 9);
					t = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					t = rdtsc() - t;


					// Read

					cached = s < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = s > memory_time_l2_threshold_copy_cb_lo[n]
						      && s < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if (!cached) l2_misses_ok_read++;

					cached = t < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = t > memory_time_l2_threshold_copy_cb_lo[n]
						      && t < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if ( cached) no_misses_ok_read++;


					// Write

					if (s < memory_time_l2_threshold_copy[n]) {
						cached = s < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (s > memory_time_l2_threshold_copy_write_lo[n]
						       && s < memory_time_l2_threshold_copy_write[0][n]);
					}

					if (!cached) l2_misses_ok_write++;

					if (t < memory_time_l2_threshold_copy[n]) {
						cached = t < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (t > memory_time_l2_threshold_copy_write_lo[n]
						       && t < memory_time_l2_threshold_copy_write[0][n]);
					}

					if (!cached) no_misses_ok_write++;
				}
			}

			memory_okr_overhead_copy[0][2][n] = l2_misses_ok_read;
			memory_okr_overhead_copy[1][2][n] = no_misses_ok_read;
			memory_okw_overhead_copy[0][2][n] = l2_misses_ok_write;
			memory_okw_overhead_copy[1][2][n] = no_misses_ok_write;


			// Destination is cached

			no_misses_ok_read = 0;
			l2_misses_ok_read = 0;
			no_misses_ok_write = 0;
			l2_misses_ok_write = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					memory_read(write_buffer, n << 9);
					s = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = rdtsc() - s;

					asm volatile("mfence");

					memory_read(write_buffer, n << 9);
					t = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					t = rdtsc() - t;


					// Read

					cached = s < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = s > memory_time_l2_threshold_copy_cb_lo[n]
						      && s < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if (!cached) l2_misses_ok_read++;

					cached = t < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = t > memory_time_l2_threshold_copy_cb_lo[n]
						      && t < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if ( cached) no_misses_ok_read++;


					// Write

					if (s < memory_time_l2_threshold_copy[n]) {
						cached = s < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (s > memory_time_l2_threshold_copy_write_lo[n]
						       && s < memory_time_l2_threshold_copy_write[0][n]);
					}

					if ( cached) l2_misses_ok_write++;

					if (t < memory_time_l2_threshold_copy[n]) {
						cached = t < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (t > memory_time_l2_threshold_copy_write_lo[n]
						       && t < memory_time_l2_threshold_copy_write[0][n]);
					}

					if ( cached) no_misses_ok_write++;
				}
			}

			memory_okr_overhead_copy[0][1][n] = l2_misses_ok_read;
			memory_okr_overhead_copy[1][1][n] = no_misses_ok_read;
			memory_okw_overhead_copy[0][1][n] = l2_misses_ok_write;
			memory_okw_overhead_copy[1][1][n] = no_misses_ok_write;


			// Source is not cached + writeback

			no_misses_ok_read = 0;
			l2_misses_ok_read = 0;
			no_misses_ok_write = 0;
			l2_misses_ok_write = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");
					for (s = 0; s < 128 * 1024; s++) dirty_buffer[s] = 0;

					s = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = rdtsc() - s;


					// Read

					cached = s < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = s > memory_time_l2_threshold_copy_cb_lo[n]
						      && s < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if (!cached) l2_misses_ok_read++;


					// Write

					if (s < memory_time_l2_threshold_copy[n]) {
						cached = s < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (s > memory_time_l2_threshold_copy_write_lo[n]
						       && s < memory_time_l2_threshold_copy_write[0][n]);
					}

					if (!cached) l2_misses_ok_write++;
				}
			}

			memory_okr_overhead_copy[2][2][n] = l2_misses_ok_read;
			memory_okw_overhead_copy[2][2][n] = l2_misses_ok_write;

			no_misses_ok_read = 0;
			l2_misses_ok_read = 0;
			no_misses_ok_write = 0;
			l2_misses_ok_write = 0;

			for (u = 0; u < max_count; u++) {
				if (buffers[u] != NULL) {

					asm volatile("wbinvd");
					asm volatile("mfence");

					for (s = 0; s < 128 * 1024; s++) dirty_buffer[s] = 0;
					memory_read(write_buffer, n << 9);

					s = rdtsc();
					memory_copy(write_buffer, buffers[u], n << 9);
					s = rdtsc() - s;


					// Read

					cached = s < memory_time_l2_threshold_copy[n];
					if (!cached) {
						cached = s > memory_time_l2_threshold_copy_cb_lo[n]
						      && s < memory_time_l2_threshold_copy_cb_hi[n];
					}

					if (!cached) l2_misses_ok_read++;


					// Write

					if (s < memory_time_l2_threshold_copy[n]) {
						cached = s < memory_time_l2_threshold_copy_write[1][n];
					}
					else {
						cached = (s > memory_time_l2_threshold_copy_write_lo[n]
						       && s < memory_time_l2_threshold_copy_write[0][n]);
					}

					if ( cached) l2_misses_ok_write++;
				}
			}

			memory_okr_overhead_copy[2][1][n] = l2_misses_ok_read;
			memory_okw_overhead_copy[2][1][n] = l2_misses_ok_write;
		}
	}

#endif /* PCMSIM_CHECK_ACCURACY */


	//
	// Cleanup
	//

	for (u = 0; u < max_count; u++) {
		if (buffers[u] != NULL) vfree(buffers[u]);
	}
	if (write_buffer != NULL) vfree(write_buffer);
	if (dirty_buffer != NULL) vfree(dirty_buffer);


	//
	// Print a report
	//

	printk("\n");
	printk("  PCMSIM Memory Settings  \n");
	printk("--------------------------\n");
	printk("\n");
	printk("Memory Bus    : %sDDR%c%s%d\n",
	       memory_ddr_version <= 1 ? " " : "",
		   memory_ddr_version <= 1 ? '-' : ('0' + memory_ddr_version),
		   memory_ddr_version <= 1 ?  "" : "-", memory_ddr_rating);
	printk("Memory Width  : %4d bytes\n", memory_row_width);
	printk("Bus Frequency : %4d MHz\n", memory_bus_mhz);
	printk("Scaling Factor: %4d\n", memory_bus_scale);
	printk("\n");
	printk("tRCD          : %4d bus cycles\n", memory_tRCD);
	printk("tRP           : %4d bus cycles\n", memory_tRP);
	printk("\n");

	printk("\n");
	printk("  PCMSIM Calibration Report  \n");
	printk("-----------------------------\n");
	printk("\n");
	printk("CPU Frequency : %4d MHz\n", cpu_khz / 1000);
	printk("Num. of trials: %4d trials\n", count);
	printk("get_ticks     : %4d cycles\n", overhead_get_ticks);
	printk("\n");
	printk("Cached reads  : %4d cycles\n", no_misses / count);
	printk("Uncached reads: %4d cycles\n", l2_misses / count);
	printk("\n");

	printk("Memory Access\n");
	printk("                 rUwU    rUwC      rU    rCwU    rCwC      rC\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   memory_overhead_copy[0][0][n],
			   memory_overhead_copy[0][1][n],
			   memory_overhead_read[0][n],
			   memory_overhead_copy[1][0][n],
			   memory_overhead_copy[1][1][n],
		       memory_overhead_read[1][n]);
	}
	printk("\n");
	printk("                 rUwB    rCwB    rBwC    rBwB\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   memory_overhead_copy[0][2][n],
			   memory_overhead_copy[1][2][n],
			   memory_overhead_copy[2][1][n],
			   memory_overhead_copy[2][2][n]);
	}
	printk("\n");

	printk("Memory Access - half-widths of 95%% prediction intervals\n");
	printk("                 rUwU    rUwC      rU    rCwU    rCwC      rC\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   hw95pi(memory_var_overhead_copy[0][0][n], count),
			   hw95pi(memory_var_overhead_copy[0][1][n], count),
			   hw95pi(memory_var_overhead_read[0][n]   , count),
			   hw95pi(memory_var_overhead_copy[1][0][n], count),
			   hw95pi(memory_var_overhead_copy[1][1][n], count),
			   hw95pi(memory_var_overhead_read[1][n]   , count));
	}
	printk("\n");
	printk("                 rUwB    rCwB    rBwC    rBwB\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   hw95pi(memory_var_overhead_copy[0][2][n], count),
			   hw95pi(memory_var_overhead_copy[1][2][n], count),
			   hw95pi(memory_var_overhead_copy[2][1][n], count),
			   hw95pi(memory_var_overhead_copy[2][2][n], count));
	}
	printk("\n");

	printk("Memory Read is Cached if:\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s     T < %4d or (T > %4d and T < %4d)\n",
			   n, n == 1 ? " " : "s",
			   memory_time_l2_threshold_copy[n],
			   memory_time_l2_threshold_copy_cb_lo[n],
			   memory_time_l2_threshold_copy_cb_hi[n]);
	}
	printk("\n");
	printk("Memory Write to a Cached Region if:\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s     T < %4d or (T > %4d and T < %4d)\n",
			   n, n == 1 ? " " : "s",
			   memory_time_l2_threshold_copy_write[1][n],
			   memory_time_l2_threshold_copy_write_lo[n],
			   memory_time_l2_threshold_copy_write[0][n]);
	}
	printk("\n");


#ifdef PCMSIM_CHECK_ACCURACY

	printk("Memory Reads - Accuracy (max = %d)\n", count);
	printk("                 rUwU    rUwC    rCwU    rCwC\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   memory_okr_overhead_copy[0][0][n],
			   memory_okr_overhead_copy[0][1][n],
			   memory_okr_overhead_copy[1][0][n],
			   memory_okr_overhead_copy[1][1][n]);
	}
	printk("\n");
	printk("                 rUwB    rCwB    rBwC    rBwB\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   memory_okr_overhead_copy[0][2][n],
			   memory_okr_overhead_copy[1][2][n],
			   memory_okr_overhead_copy[2][1][n],
			   memory_okr_overhead_copy[2][2][n]);
	}
	printk("\n");

	printk("Memory Writes - Accuracy (max = %d)\n", count);
	printk("                 rUwU    rUwC    rCwU    rCwC\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   memory_okw_overhead_copy[0][0][n],
			   memory_okw_overhead_copy[0][1][n],
			   memory_okw_overhead_copy[1][0][n],
			   memory_okw_overhead_copy[1][1][n]);
	}
	printk("\n");
	printk("                 rUwB    rCwB    rBwC    rBwB\n");
	for (n = 1; n <= PCMSIM_MEM_SECTORS; n++) {
		printk("%4d sector%s %8d%8d%8d%8d\n",
			   n, n == 1 ? " " : "s",
			   memory_okw_overhead_copy[0][2][n],
			   memory_okw_overhead_copy[1][2][n],
			   memory_okw_overhead_copy[2][1][n],
			   memory_okw_overhead_copy[2][2][n]);
	}
	printk("\n");

#endif /* PCMSIM_CHECK_ACCURACY */
}


/**
 * Determine whether the given buffer was present in its entirety
 * in the L2 cache before this function has been called. This function
 * loads the buffer to the cache as a part of its function, and in order
 * to function properly, it assumes that the buffer offset and the size
 * are aligned to a cache-line size.
 */
int memory_was_cached(const void* buffer, size_t size)
{
	unsigned t = memory_time_read(buffer, size);
	return t < memory_time_l2_threshold;
}
