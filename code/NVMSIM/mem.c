#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/timex.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>

#define __NVM_MEM_NO_EXTERN
#include "mem.h"


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