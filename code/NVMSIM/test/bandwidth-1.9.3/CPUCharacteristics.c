
/*============================================================================
  CPUCharacteristics, object-oriented C class providing an interface
  to assembly language routines.
  Copyright (C) 2019 by Zack T Smith.

  This class is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This class is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public License
  along with this software.  If not, see <http://www.gnu.org/licenses/>.

  The author may be reached at 1@zsmith.co.
 *===========================================================================*/

#include <sys/stat.h>
#include <unistd.h>

#include "routines.h"
#include "CPUCharacteristics.h"
#include "Console.h"

extern char *strcpy(char *s1, const char *s2);
extern int strcmp(const char *s1, const char *s2);

CPUCharacteristicsClass* _CPUCharacteristicsClass = NULL;

extern Console* console;

static void CPUCharacteristics_printCharacteristics (CPUCharacteristics *self)
{
	$(console, print, "CPU family: ");
	$(console, println, self->cpu_family);

#ifdef x86
	$(console, print, "CPU features: ");
	if (self->has_mmx)	$(console, print, "MMX ");
	if (self->has_mmxext)	$(console, print, "MMXEXT ");
	if (self->has_sse)	$(console, print, "SSE ");
	if (self->has_sse2)	$(console, print, "SSE2 ");
	if (self->has_sse3)	$(console, print, "SSE3 ");
	if (self->has_ssse3)	$(console, print, "SSSE3 ");
	if (self->has_sse4a)	$(console, print, "SSE4A ");
	if (self->has_sse41)	$(console, print, "SSE4.1 ");
	if (self->has_sse42)	$(console, print, "SSE4.2 ");
	if (self->has_aes)	$(console, print, "AES ");
	if (self->has_sha)	$(console, print, "SHA ");
	if (self->has_sgx)	$(console, print, "SGX ");
	if (self->has_avx)	$(console, print, "AVX ");
	if (self->has_avx2)	$(console, print, "AVX2 ");
	if (self->has_avx512)	$(console, print, "AVX512 ");
	if (self->has_adx)	$(console, print, "ADX ");
	if (self->has_hyperthreading)	$(console, print, "HTT ");
	if (self->has_nx)	$(console, print, "NX ");
	if (self->has_cet)	$(console, print, "CET ");
	if (self->has_64bit) {
		if (!self->is_amd)
			$(console, print, "Intel64 ");
		else 
			$(console, print, "LongMode ");
	}
	$(console, newline);
#endif
	
	if (self->running_in_hypervisor) {
		$(console, println, "Hypervisor is present; you're running in a VM.");
	}

	$(console, newline);
}

static void CPUCharacteristics_printCacheInfo (CPUCharacteristics *self)
{
#ifdef x86
#ifndef __WIN64__
	uint32_t cache_info[4];
	int i = 0;
	while (1) {
		get_cpuid_cache_info (cache_info, i);
		if (!(cache_info[0] & 31))
			break;

		// printf ("Cache info %d = 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", i, cache_info [0], cache_info [1], cache_info [2], cache_info [3]);

		$(console, print, "Cache ");
		$(console, print_int, i);
		$(console, print, ": ");
		
		switch ((cache_info[0] >> 5) & 7) {
		case 1: $(console, print, "L1 "); break;
		case 2: $(console, print, "L2 "); break;
		case 3: $(console, print, "L3 "); break;
		}
		switch (cache_info[0] & 31) {
		case 1: $(console, print, "data cache,        "); break;
		case 2: $(console, print, "instruction cache, "); break;
		case 3: $(console, print, "unified cache,     "); break;
		}
		uint32_t n_ways = 1 + (cache_info[1] >> 22);
		uint32_t line_size = 1 + (cache_info[1] & 2047);
		uint32_t n_sets = 1 + cache_info[2];
		printf ("line size %d, ", line_size);
		printf ("%2d-way%s, ", n_ways, n_ways>1 ? "s" : "");
		printf ("%5d sets, ", n_sets);
		unsigned size = (n_ways * line_size * n_sets) >> 10;
		printf ("size %dk ", size);
		$(console, newline);
		i++;
	} 
#endif
#endif
}

static char sysInfo[1000];
static char *CPUCharacteristics_getCPUString (CPUCharacteristics *self)
{
#ifdef __WIN32__
	strcpy (sysInfo, "Windows 32-bit");
#endif
#ifdef __WIN64__
	strcpy (sysInfo, "Windows 64-bit");
#endif
#ifdef __linux__
	struct stat st;
	unlink ("/tmp/.brandstring");
	if (!stat ("/proc/cpuinfo", &st)) {
		system ("grep 'model name' /proc/cpuinfo | sed 's/^.*: //' | head -1 > /tmp/.brandstring");
	} else {
		$(console, println, "CPU information is not available (/proc/cpuinfo).");
		$(console, flush);
	}
	system ("uname -sr >> /tmp/.brandstring");
#endif
#ifdef __APPLE__
	system ("sysctl machdep.cpu.brand_string | sed 's/^.*: //' > /tmp/.brandstring");
	system ("uname -s -r >> /tmp/.brandstring");
#endif
#if defined(__APPLE__) || defined(__linux__)
	FILE *fileSysInfo = fopen ("/tmp/.brandstring", "r");
	if (fileSysInfo) {
		size_t nBytes;
		if (0 < (nBytes = fread (sysInfo, 1, sizeof(sysInfo)-1, fileSysInfo))) {
			sysInfo[nBytes] = 0;
			
			int i=0;
			while (sysInfo[i]) {
				if (sysInfo[i] == '\n')
					sysInfo[i] = ' ';
				i++;
			}
		}
		fclose (fileSysInfo);
	}
#endif
	return sysInfo;
}

static void CPUCharacteristics_destroy (CPUCharacteristics *self)
{
}

static void CPUCharacteristics_describe (CPUCharacteristics *self, FILE *file)
{
}

CPUCharacteristicsClass* CPUCharacteristicsClass_prepare ()
{
	PREPARE_CLASS_STRUCT(CPUCharacteristics,Object)

	SET_OVERRIDDEN_METHOD_POINTER(CPUCharacteristics,describe);
	SET_OVERRIDDEN_METHOD_POINTER(CPUCharacteristics,destroy);

	SET_METHOD_POINTER(CPUCharacteristics,printCacheInfo);
	SET_METHOD_POINTER(CPUCharacteristics,printCharacteristics);
	SET_METHOD_POINTER(CPUCharacteristics,getCPUString);
	
        VALIDATE_CLASS_STRUCT(_CPUCharacteristicsClass);
	return _CPUCharacteristicsClass;
}

CPUCharacteristics *CPUCharacteristics_init (CPUCharacteristics *self)
{
 	if (!_CPUCharacteristicsClass)
                CPUCharacteristicsClass_prepare ();

        Object_init ((Object*) self);
        self->is_a = _CPUCharacteristicsClass;

#ifdef __arm__
	self->is_arm = true;
	ooc_strncpy (self->cpu_family, "ARM", sizeof(self->cpu_family)-1);
#endif

#ifdef x86
	uint32_t ecx = get_cpuid1_ecx ();
	uint32_t edx = get_cpuid1_edx ();
	self->has_mmx = edx & CPUID1_EDX_MMX;
	self->has_sse = edx & CPUID1_EDX_SSE;
	self->has_sse2 = edx & CPUID1_EDX_SSE2;
	self->has_sse3 = ecx & CPUID1_ECX_SSE3;
	self->has_ssse3 = ecx & CPUID1_ECX_SSSE3;
	self->has_sse41 = ecx & CPUID1_ECX_SSE41;
	self->has_sse42 = ecx & CPUID1_ECX_SSE42;
	self->has_aes = ecx & CPUID1_ECX_AES;
	self->has_avx = ecx & CPUID1_ECX_AVX;
	self->has_hyperthreading = edx & CPUID1_EDX_HTT;

	self->running_in_hypervisor = ecx & CPUID1_ECX_HYPER_GUEST;

	ecx = get_cpuid7_ecx (); 
	self->has_cet = ecx & CPUID7_ECX_CET;

	uint32_t ebx = get_cpuid7_ebx (); 
	self->has_adx = ebx & CPUID7_EBX_ADX;
	self->has_sha = ebx & CPUID7_EBX_SHA;
	self->has_sgx = ebx & CPUID7_EBX_SGX;
	if (self->has_avx) {
		self->has_avx2 = ebx & CPUID7_EBX_AVX2;
		self->has_avx512 = ebx & CPUID7_EBX_AVX512F;
	}

	self->has_sse4a = 0;
	self->has_64bit = 0;

	static char family [17];
	get_cpuid_family (family);
	family [16] = 0;
	ooc_strncpy (self->cpu_family, family, sizeof(self->cpu_family)-1);

	uint32_t ecx2 = get_cpuid_80000001_ecx ();
	uint32_t edx2 = get_cpuid_80000001_edx ();

	self->has_nx = edx2 & CPUID80000001_EDX_NX;
	self->has_mmxext = edx2 & CPUID80000001_EDX_MMXEXT;
	self->has_64bit = edx2 & CPUID80000001_EDX_INTEL64;

	if (!strcmp ("AuthenticAMD", family)) {
		self->is_amd = true;
		self->has_sse4a = ecx2 & CPUID80000001_ECX_SSE4A;

		// NOTE: It's been observed that AMD CPUs have issues running
		// the AVX part of this program, so I'm disabling it for now.
		self->has_avx = 0;
		self->has_avx2 = 0;
		self->has_avx512 = 0;
	}
	else if (!strcmp ("GenuineIntel", family)) {
		self->is_intel = true;
	}
#endif
        return self;
}

