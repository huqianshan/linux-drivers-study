;============================================================================
;  bandwidth
;  Copyright (C) 2015,2019 by Zack T Smith.
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License version 2 as published by
;  the Free Software Foundation.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software
;  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
;
;  The author may be reached at 1@zsmith.co.
;=============================================================================

bits	32
cpu	ia64

global	_get_cpuid1_ecx
global	_get_cpuid1_edx
global	_get_cpuid_80000001_ecx
global	_get_cpuid_80000001_edx
global	_get_cpuid_family
global  _get_cpuid_cache_info
global	_get_cpuid7_ebx
global	_get_cpuid7_ecx

global	get_cpuid1_ecx
global	get_cpuid1_edx
global	get_cpuid_80000001_ecx
global	get_cpuid_80000001_edx
global	get_cpuid_family
global  get_cpuid_cache_info
global	get_cpuid7_ebx
global	get_cpuid7_ecx

	section .text

;------------------------------------------------------------------------------
; Name: 	get_cpuid_cache_info
; 
get_cpuid_cache_info:
_get_cpuid_cache_info:
	push	ebp
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 4
	mov	ecx, [esp + 16 + 4 + 4]
	cpuid
	mov	ebp, eax
	mov	eax, [esp + 16 + 4]
	mov	[eax], ebp
	mov	[eax+4], ebx
	mov	[eax+8], ecx
	mov	[eax+12], edx
	pop	edx
	pop	ecx
	pop	ebx
	pop	ebp
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid_family
; 
get_cpuid_family:
_get_cpuid_family:
	push	ebx
	push 	ecx
	push 	edx
	xor	eax, eax
	cpuid
	mov	eax, [esp + 12 + 4]
	mov	[eax], ebx
	mov	[eax+4], edx
	mov	[eax+8], ecx
	mov	byte [eax+12], 0
	pop	edx
	pop	ecx
	pop	ebx
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid1_ecx
; 
get_cpuid1_ecx:
_get_cpuid1_ecx:
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 1
	cpuid
	mov	eax, ecx
	pop	edx
	pop	ecx
	pop	ebx
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid7_ebx
; 
get_cpuid7_ebx:
_get_cpuid7_ebx:
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 7
	xor	ecx, ecx
	cpuid
        mov	eax, ebx
	pop	edx
	pop	ecx
	pop	ebx
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid7_ecx
; 
get_cpuid7_ecx:
_get_cpuid7_ecx:
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 7
	xor	ecx, ecx
	cpuid
        mov	eax, ecx
	pop	edx
	pop	ecx
	pop	ebx
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid_80000001_ecx
; 
get_cpuid_80000001_ecx:
_get_cpuid_80000001_ecx:
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 0x80000001
	cpuid
	mov	eax, ecx
	pop	edx
	pop	ecx
	pop	ebx
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid_80000001_edx
; 
get_cpuid_80000001_edx:
_get_cpuid_80000001_edx:
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 0x80000001
	cpuid
	mov	eax, edx
	pop	edx
	pop	ecx
	pop	ebx
	ret

;------------------------------------------------------------------------------
; Name:		get_cpuid1_edx
; 
get_cpuid1_edx:
_get_cpuid1_edx:
	push	ebx
	push 	ecx
	push 	edx
	mov	eax, 1
	cpuid
	mov	eax, edx
	pop	edx
	pop	ecx
	pop	ebx
	ret

