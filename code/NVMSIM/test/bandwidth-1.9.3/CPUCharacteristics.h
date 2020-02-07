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

#ifndef _OOC_CPUCHARACTERISTICS_H
#define _OOC_CPUCHARACTERISTICS_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "OOC/Object.h"
#include "timing.h"

#ifndef __arm__
#if defined(__i386__) || defined(__x86_64__) || defined(_WIN64) || defined(_WIN32) ||defined(__WIN32__) || defined(__WIN64__) || defined(_M_IX86) || defined(__MINGW32__) || defined(__i386)
#define x86
#endif
#endif

#if defined(__x86_64__) || defined(_WIN64) || defined(__WIN64__)
#define IS_64BIT
#endif

#define DECLARE_CPUCHARACTERISTICS_INSTANCE_VARS(TYPE_POINTER) \
	char cpu_family [32];\
	bool is_intel;\
	bool is_amd;\
	bool is_arm;\
	uint32_t has_hyperthreading;\
	uint32_t has_mmx;\
	uint32_t has_mmxext;\
	uint32_t has_sse;\
	uint32_t has_sse2;\
	uint32_t has_sse3;\
	uint32_t has_ssse3;\
	uint32_t has_sse4a;\
	uint32_t has_sse41;\
	uint32_t has_sse42;\
	uint32_t has_aes;\
	uint32_t has_sha;\
	uint32_t has_sgx;\
	uint32_t has_avx;\
	uint32_t has_avx2;\
	uint32_t has_avx512;\
	uint32_t has_64bit;\
	uint32_t has_nx;\
	uint32_t has_adx;\
	uint32_t has_cet;\
	uint32_t running_in_hypervisor;\

#define DECLARE_CPUCHARACTERISTICS_METHODS(TYPE_POINTER) \
	void (*printCharacteristics) (TYPE_POINTER);\
	void (*printCacheInfo) (TYPE_POINTER);\
	char *(*getCPUString) (TYPE_POINTER);

struct cpucharacteristics;

typedef struct cpucharacteristicsclass {
	DECLARE_OBJECT_CLASS_VARS
        DECLARE_OBJECT_METHODS(struct cpucharacteristics*)
        DECLARE_CPUCHARACTERISTICS_METHODS(struct cpucharacteristics*)
} CPUCharacteristicsClass;

extern CPUCharacteristicsClass *_CPUCharacteristicsClass;

typedef struct cpucharacteristics {
        CPUCharacteristicsClass *is_a;
	DECLARE_OBJECT_INSTANCE_VARS(struct cpucharacteristics*)
	DECLARE_CPUCHARACTERISTICS_INSTANCE_VARS(struct cpucharacteristics*)
} CPUCharacteristics;

extern CPUCharacteristics *CPUCharacteristics_init (CPUCharacteristics *self);

#endif
