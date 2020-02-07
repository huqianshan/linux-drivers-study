# ============================================================================
#  bandwidth 0.23, a benchmark to estimate memory transfer bandwidth.
#  ARM assembly module.
#  Copyright (C) 2010 by Zack T Smith.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#  The author may be reached at 1@zsmith.co.
# =============================================================================

# Version 0.8

# Note, some instructions are not supported by the ARM CPU in the Raspberry pi 2.

.section code ,

.globl _Writer
.globl _WriterVector
.globl _RandomWriter
.globl _RandomWriterVector
.globl _Reader
.globl _ReaderVector
.globl _RandomReader
.globl _RandomReaderVector
.globl _RegisterToRegister
.globl _RegisterToRegisterVector
.globl _StackReader
.globl _StackWriter

.text

#-----------------------------------------------------------------------------
# Name: 	Writer
# Purpose:	Performs sequential write into memory, as fast as possible.
# Params:
#	r0 = address
#	r1 = length, multiple of 256
#	r2 = count
# 	r3 = value to write
#-----------------------------------------------------------------------------
_Writer:
	stmfd	sp!,{r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

# r4 = temp
# r5 = temp

	and	r1, #0xffffff80
	mov	r4, r0
	mov	r5, r1

	mov	r6, r3
	mov	r7, r3
	mov	r8, r3
	mov	r9, r3
	mov	r10, r3
	mov	r11, r3
	mov	r12, r3

.L0:
	mov	r0, r4
	mov	r1, r5

.L1:
# Does 64 transfers, 4 bytes each = 256 bytes total.
# The "stmia" instruction automatically increments r0.
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
        stmia   r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }

	sub	r1, #256
	cmp	r1, #0
	bne	.L1

	sub	r2, #1
	cmp	r2, #0
	bne	.L0

# return.
	ldmfd	sp!,{r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}

#-----------------------------------------------------------------------------
# Name: 	WriterVector
# Purpose:	Performs sequential write into memory, as fast as possible.
# Params:
#	r0 = address
#	r1 = length, multiple of 256
#	r2 = count
# 	r3 = value to write
#-----------------------------------------------------------------------------
_WriterVector:
	stmfd	sp!,{r4, r5, lr}

# r4 = temp
# r5 = temp

	and	r1, #0xffffff80
	mov	r4, r0
	mov	r5, r1

.L0v:
	mov	r0, r4
	mov	r1, r5

.L1v:
# Does 32 transfers, 8 bytes each = 256 bytes total.
# The "vstmia" instruction automatically increments r0.

        vstmia   r0!, { d0 - d7 }
        vstmia   r0!, { d0 - d7 }
        vstmia   r0!, { d0 - d7 }
        vstmia   r0!, { d0 - d7 }

	sub	r1, #256
	cmp	r1, #0
	bne	.L1v

	sub	r2, #1
	cmp	r2, #0
	bne	.L0v

# return.
	ldmfd	sp!,{r4, r5, pc}

#-----------------------------------------------------------------------------
# Name: 	Reader
# Purpose:	Performs sequential reads from memory, as fast as possible.
# Params:
#	r0 = address
#	r1 = length, multiple of 256
#	r2 = count
#-----------------------------------------------------------------------------
_Reader:
	stmfd	sp!,{r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

# r3 = temp

	and	r1, #0xffffff80
	mov	r4, r0
	mov	r5, r1

.L2:
	mov	r0, r4
	mov	r1, r5

.L3:
# Does 64 transfers, 4 bytes each = 256 bytes total.
# The "ldmia" instruction automatically increments r0.

	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }
	ldmia	r0!, { r3, r6, r7, r8, r9, r10, r11, r12 }

	sub	r1, #256
	cmp	r1, #0
	bne	.L3

	sub	r2, #1
	cmp	r2, #0
 	bne	.L2

# return.
	ldmfd	sp!,{r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}

#-----------------------------------------------------------------------------
# Name: 	ReaderVector
# Purpose:	Performs sequential reads from memory, as fast as possible.
# Params:
#	r0 = address
#	r1 = length, multiple of 256
#	r2 = count
#-----------------------------------------------------------------------------
_ReaderVector:
	stmfd	sp!,{r4, r5, lr}

# r3 = temp

	and	r1, #0xffffff80
	mov	r4, r0
	mov	r5, r1

.L2v:
	mov	r0, r4
	mov	r1, r5

.L3v:
# The "vldmia" instruction automatically increments r0.
# 8 bytes * 8 registers = 64 bytes
#	vldmia	r0!, { d0-d7 } 
#	vldmia	r0!, { d0-d7 }
#	vldmia	r0!, { d0-d7 }
#	vldmia	r0!, { d0-d7 }

# 128bit=16 bytes * 16 = 256
	vldmia	r0!, {q0,q1,q2,q3,q4,q5,q6,q7}
	vldmia	r0!, {q0,q1,q2,q3,q4,q5,q6,q7}

	sub	r1, #256
	cmp	r1, #0
	bne	.L3v

	sub	r2, #1
	cmp	r2, #0
 	bne	.L2v

# return.
	ldmfd	sp!,{r4, r5, pc}

#-----------------------------------------------------------------------------
# Name: 	RandomWriter
# Purpose:	Performs random write into memory, as fast as possible.
# Params:
# 	r0 = pointer to array of chunk pointers
# 	r1 = # of 256-byte chunks
# 	r2 = # loops to do
# 	r3 = value to write
#-----------------------------------------------------------------------------
_RandomWriter:
	stmfd	sp!,{r4, r5, lr}

# r4 = temp
# r5 = temp

.L4:
	mov	r5, #0

.L5:
# Get pointer to chunk in memory.
	ldr	r4, [r0, r5, LSL #2]

# Does 64 transfers, 4 bytes each = 256 bytes total.

	str	r3, [r4, #160]
	str	r3, [r4, #232]
	str	r3, [r4, #224]
	str	r3, [r4, #96]
	str	r3, [r4, #164]
	str	r3, [r4, #76]
	str	r3, [r4, #100]
	str	r3, [r4, #220]
	str	r3, [r4, #248]
	str	r3, [r4, #104]
	str	r3, [r4, #4]
	str	r3, [r4, #136]
	str	r3, [r4, #112]
	str	r3, [r4, #200]
	str	r3, [r4, #12]
	str	r3, [r4, #128]
	str	r3, [r4, #148]
	str	r3, [r4, #196]
	str	r3, [r4, #216]
	str	r3, [r4]
	str	r3, [r4, #84]
	str	r3, [r4, #140]
	str	r3, [r4, #204]
	str	r3, [r4, #184]
	str	r3, [r4, #124]
	str	r3, [r4, #48]
	str	r3, [r4, #64]
	str	r3, [r4, #212]
	str	r3, [r4, #240]
	str	r3, [r4, #236]
	str	r3, [r4, #24]
	str	r3, [r4, #252]
	str	r3, [r4, #68]
	str	r3, [r4, #20]
	str	r3, [r4, #72]
	str	r3, [r4, #32]
	str	r3, [r4, #28]
	str	r3, [r4, #52]
	str	r3, [r4, #244]
	str	r3, [r4, #180]
	str	r3, [r4, #80]
	str	r3, [r4, #60]
	str	r3, [r4, #8]
	str	r3, [r4, #56]
	str	r3, [r4, #208]
	str	r3, [r4, #228]
	str	r3, [r4, #40]
	str	r3, [r4, #172]
	str	r3, [r4, #120]
	str	r3, [r4, #176]
	str	r3, [r4, #108]
	str	r3, [r4, #132]
	str	r3, [r4, #16]
	str	r3, [r4, #44]
	str	r3, [r4, #92]
	str	r3, [r4, #168]
	str	r3, [r4, #152]
	str	r3, [r4, #156]
	str	r3, [r4, #188]
	str	r3, [r4, #36]
	str	r3, [r4, #88]
	str	r3, [r4, #116]
	str	r3, [r4, #192]
	str	r3, [r4, #144]

	add	r5, #1
	cmp	r5, r1
	bne	.L5

	sub	r2, #1
	cmp	r2, #0
	bne	.L4

# return.
	ldmfd	sp!,{r4, r5, pc}

#-----------------------------------------------------------------------------
# Name: 	RandomWriterVector
# Purpose:	Performs random write into memory, as fast as possible.
# Params:
# 	r0 = pointer to array of chunk pointers
# 	r1 = # of 256-byte chunks
# 	r2 = # loops to do
# 	r3 = value to write
#-----------------------------------------------------------------------------
_RandomWriterVector:
	stmfd	sp!,{r4, r5, lr}

# r4 = temp
# r5 = temp

.L4v:
	mov	r5, #0

.L5v:
# Get pointer to chunk in memory.
	ldr	r4, [r0, r5, LSL #2]

# Does 32 transfers, 8 bytes each = 256 bytes total.

	vstr	d0, [r4, #56]
	vstr	d0, [r4, #136]
	vstr	d0, [r4, #200]
	vstr	d0, [r4, #80]
	vstr	d0, [r4, #16]
	vstr	d0, [r4, #64]
	vstr	d0, [r4, #192]
	vstr	d0, [r4, #240]
	vstr	d0, [r4, #24]
	vstr	d0, [r4, #104]
	vstr	d0, [r4, #192]
	vstr	d0, [r4, #168]
	vstr	d0, [r4, #96]
	vstr	d0, [r4, #16]
	vstr	d0, [r4, #152]
	vstr	d0, [r4, #56]
	vstr	d0, [r4, #184]
	vstr	d0, [r4, #136]
	vstr	d0, [r4, #192]
	vstr	d0, [r4, #160]
	vstr	d0, [r4, #120]
	vstr	d0, [r4, #136]
	vstr	d0, [r4, #104]
	vstr	d0, [r4, #40]
	vstr	d0, [r4, #192]
	vstr	d0, [r4, #168]
	vstr	d0, [r4, #88]
	vstr	d0, [r4, #176]
	vstr	d0, [r4, #64]
	vstr	d0, [r4, #56]
	vstr	d0, [r4, #200]
	vstr	d0, [r4, #144]

	add	r5, #1
	cmp	r5, r1
	bne	.L5v

	sub	r2, #1
	cmp	r2, #0
	bne	.L4v

# return.
	ldmfd	sp!,{r4, r5, pc}

#-----------------------------------------------------------------------------
# Name: 	RandomReader
# Purpose:	Performs random reads from memory, as fast as possible.
# Params:
# 	r0 = pointer to array of chunk pointers
# 	r1 = # of 256-byte chunks
# 	r2 = # loops to do
#-----------------------------------------------------------------------------
_RandomReader:
	stmfd	sp!,{r4, r5, lr}

# r3 = temp
# r4 = temp
# r5 = temp

.L6:
	mov	r5, #0

.L7:
# Get pointer to chunk in memory.
	ldr	r4, [r0, r5, LSL #2]

# Does 64 transfers, 4 bytes each = 256 bytes total.

	ldr	r3, [r4, #160]
	ldr	r3, [r4, #232]
	ldr	r3, [r4, #224]
	ldr	r3, [r4, #96]
	ldr	r3, [r4, #164]
	ldr	r3, [r4, #76]
	ldr	r3, [r4, #100]
	ldr	r3, [r4, #220]
	ldr	r3, [r4, #248]
	ldr	r3, [r4, #104]
	ldr	r3, [r4, #4]
	ldr	r3, [r4, #136]
	ldr	r3, [r4, #112]
	ldr	r3, [r4, #200]
	ldr	r3, [r4, #12]
	ldr	r3, [r4, #128]
	ldr	r3, [r4, #148]
	ldr	r3, [r4, #196]
	ldr	r3, [r4, #216]
	ldr	r3, [r4]
	ldr	r3, [r4, #84]
	ldr	r3, [r4, #140]
	ldr	r3, [r4, #204]
	ldr	r3, [r4, #184]
	ldr	r3, [r4, #124]
	ldr	r3, [r4, #48]
	ldr	r3, [r4, #64]
	ldr	r3, [r4, #212]
	ldr	r3, [r4, #240]
	ldr	r3, [r4, #236]
	ldr	r3, [r4, #24]
	ldr	r3, [r4, #252]
	ldr	r3, [r4, #68]
	ldr	r3, [r4, #20]
	ldr	r3, [r4, #72]
	ldr	r3, [r4, #32]
	ldr	r3, [r4, #28]
	ldr	r3, [r4, #52]
	ldr	r3, [r4, #244]
	ldr	r3, [r4, #180]
	ldr	r3, [r4, #80]
	ldr	r3, [r4, #60]
	ldr	r3, [r4, #8]
	ldr	r3, [r4, #56]
	ldr	r3, [r4, #208]
	ldr	r3, [r4, #228]
	ldr	r3, [r4, #40]
	ldr	r3, [r4, #172]
	ldr	r3, [r4, #120]
	ldr	r3, [r4, #176]
	ldr	r3, [r4, #108]
	ldr	r3, [r4, #132]
	ldr	r3, [r4, #16]
	ldr	r3, [r4, #44]
	ldr	r3, [r4, #92]
	ldr	r3, [r4, #168]
	ldr	r3, [r4, #152]
	ldr	r3, [r4, #156]
	ldr	r3, [r4, #188]
	ldr	r3, [r4, #36]
	ldr	r3, [r4, #88]
	ldr	r3, [r4, #116]
	ldr	r3, [r4, #192]
	ldr	r3, [r4, #144]

	add	r5, #1
	cmp	r5, r1
	bne	.L7

	sub	r2, #1
	cmp	r2, #0
	bne	.L6

# return.
	ldmfd	sp!,{r4, r5, pc}

#-----------------------------------------------------------------------------
# Name: 	RandomReaderVector
# Purpose:	Performs random reads from memory, as fast as possible.
# Params:
# 	r0 = pointer to array of chunk pointers
# 	r1 = # of 256-byte chunks
# 	r2 = # loops to do
#-----------------------------------------------------------------------------
_RandomReaderVector:
	stmfd	sp!,{r4, r5, lr}

# r3 = temp
# r4 = temp
# r5 = temp

.L6v:
	mov	r5, #0

.L7v:
# Get pointer to chunk in memory.
	ldr	r4, [r0, r5, LSL #2]

# Does 64 transfers, 4 bytes each = 256 bytes total.

	vldr	d0, [r4, #56]
	vldr	d0, [r4, #136]
	vldr	d0, [r4, #200]
	vldr	d0, [r4, #80]
	vldr	d0, [r4, #16]
	vldr	d0, [r4, #64]
	vldr	d0, [r4, #192]
	vldr	d0, [r4, #240]
	vldr	d0, [r4, #24]
	vldr	d0, [r4, #104]
	vldr	d0, [r4, #192]
	vldr	d0, [r4, #168]
	vldr	d0, [r4, #96]
	vldr	d0, [r4, #16]
	vldr	d0, [r4, #152]
	vldr	d0, [r4, #56]
	vldr	d0, [r4, #184]
	vldr	d0, [r4, #136]
	vldr	d0, [r4, #192]
	vldr	d0, [r4, #160]
	vldr	d0, [r4, #120]
	vldr	d0, [r4, #136]
	vldr	d0, [r4, #104]
	vldr	d0, [r4, #40]
	vldr	d0, [r4, #192]
	vldr	d0, [r4, #168]
	vldr	d0, [r4, #88]
	vldr	d0, [r4, #176]
	vldr	d0, [r4, #64]
	vldr	d0, [r4, #56]
	vldr	d0, [r4, #200]
	vldr	d0, [r4, #144]

	add	r5, #1
	cmp	r5, r1
	bne	.L7v

	sub	r2, #1
	cmp	r2, #0
	bne	.L6v

# return.
	ldmfd	sp!,{r4, r5, pc}

#-----------------------------------------------------------------------------
# Name: 	RegisterToRegister
# Purpose:	Performs register-to-register transfers.
# Params:
#	r0 = count
#-----------------------------------------------------------------------------
_RegisterToRegister:
	stmfd	sp!,{lr}

# r1 = temp

.L8:
# Does 64 transfers, 4 bytes each = 256 bytes total.
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9
	mov	r2, r1
	mov	r2, r3
	mov	r2, r4
	mov	r2, r5
	mov	r2, r6
	mov	r2, r7
	mov	r2, r8
	mov	r2, r9
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9
	mov	r1, r2
	mov	r1, r3
	mov	r1, r4
	mov	r1, r5
	mov	r1, r6
	mov	r1, r7
	mov	r1, r8
	mov	r1, r9

	sub	r0, #1
	cmp	r0, #0
	bne	.L8

# return.
	ldmfd	sp!,{pc}

#-----------------------------------------------------------------------------
# Name: 	RegisterToRegisterVector
# Purpose:	Performs register-to-register transfers.
# Params:
#	r0 = count
#-----------------------------------------------------------------------------
_RegisterToRegisterVector:
	stmfd	sp!,{lr}

# r1 = temp

.L8v:
# Does 32 transfers, 8 bytes each = 256 bytes total.

	vmov	d1, d2
	vmov	d1, d3
	vmov	d1, d4
	vmov	d1, d5
	vmov	d1, d6
	vmov	d1, d7
	vmov	d1, d8
	vmov	d1, d9
	vmov	d2, d1
	vmov	d2, d3
	vmov	d2, d4
	vmov	d2, d5
	vmov	d2, d6
	vmov	d2, d7
	vmov	d2, d8
	vmov	d2, d9
	vmov	d1, d2
	vmov	d1, d3
	vmov	d1, d4
	vmov	d1, d5
	vmov	d1, d6
	vmov	d1, d7
	vmov	d1, d8
	vmov	d1, d9
	vmov	d2, d1
	vmov	d2, d3
	vmov	d2, d4
	vmov	d2, d5
	vmov	d2, d6
	vmov	d2, d7
	vmov	d2, d8
	vmov	d2, d9

	sub	r0, #1
	cmp	r0, #0
	bne	.L8v

# return.
	ldmfd	sp!,{pc}

#-----------------------------------------------------------------------------
# Name: 	StackReader
# Purpose:	Performs stack-to-register transfers.
# Params:
#	r0 = count
#-----------------------------------------------------------------------------
_StackReader:
	stmfd	sp!,{lr}

# r1 = temp

	sub	sp, #32
.L9:
# Does 64 transfers, 4 bytes each = 256 bytes total.

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	ldr	r1, [sp]
	ldr	r1, [sp, #4]
	ldr	r1, [sp, #8]
	ldr	r1, [sp, #12]
	ldr	r1, [sp, #16]
	ldr	r1, [sp, #20]
	ldr	r1, [sp, #24]
	ldr	r1, [sp, #28]

	sub	r0, #1
	cmp	r0, #0
	bne	.L9

	add	sp, #32

# return.
	ldmfd	sp!,{pc}

#-----------------------------------------------------------------------------
# Name: 	StackWriter
# Purpose:	Performs register-to-stack transfers.
# Params:
#	r0 = count
#-----------------------------------------------------------------------------
_StackWriter:
	stmfd	sp!,{lr}

# r1 = temp

	sub	sp, #32
.L10:
# Does 64 transfers, 4 bytes each = 256 bytes total.

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r1, [sp, #20]
	str	r1, [sp, #24]
	str	r1, [sp, #28]

	sub	r0, #1
	cmp	r0, #0
	bne	.L10

	add	sp, #32

# return.
	ldmfd	sp!,{pc}

