/*
 * util.c
 * PCM Simulator: Miscellaneous utilities
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

#include <asm/uaccess.h>

#define __PCMSIM_UTIL_NO_EXTERN
#include "util.h"


/**
 * The overhead of get_ticks()
 */
unsigned overhead_get_ticks = 0;


/**
 * Return the current value of the processor's tick counter
 */
u64 get_ticks(void)
{
	unsigned a, d;

	// Flush the pipeline
	
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


	// Read the number of ticks

	asm volatile("rdtsc" : "=a" (a), "=d" (d));

	return (((u64)a) | (((u64)d) << 32));
}


/**
 * Return the current value of the processor's tick counter, but do not flush the pipeline
 */
/*u64 rdtsc(void)
{
	unsigned a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return (((u64)a) | (((u64)d) << 32));
}*/


/**
 * Calibrate the timers in utilities
 */
void util_calibrate(void)
{
	unsigned max_count = 128;
	unsigned u, s, t;


	// Measure the overhead of get_ticks()

	t = 0;
	for (u = 0; u < max_count; u++) {
		s  = get_ticks();
		t += get_ticks() - s;
	}

	overhead_get_ticks = t / max_count;
}


/**
 * Calculate integer square root of a 32-bit integer
 */
unsigned int sqrt32(unsigned long n)
{
	// http://www.codecodex.com/wiki/Calculate_an_integer_square_root#C

	unsigned int c = 0x8000;
	unsigned int g = 0x8000;

	for (;;) {
		if (g*g > n) g ^= c;
		c >>= 1;
		if (c == 0) return g;
		g |= c;
	}
}
