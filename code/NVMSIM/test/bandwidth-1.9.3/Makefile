#============================================================================
# bandwidth, a benchmark to estimate memory transfer bandwidth.
# Copyright (C) 2005-2019 by Zack T Smith.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# The author may be reached at veritas@comcast.net.
#============================================================================

CFLAGS= -g -Wno-conditional-type-mismatch
CC=gcc -Wall -I.
SRC=main.c timing.c CPUCharacteristics.c
OBJ=main.o timing.o CPUCharacteristics.o
LIB= 
AS=nasm
ARM_AS=as
ARM_CC=gcc
# Note: Mac 32-bit no longer supported by Mac OS.

OOCLIB32=OOC/OOC32.a
OOCLIB64=OOC/OOC64.a

UNAMESYS := $(shell uname -s)
UNAMEPROC := $(shell uname -p)

ifeq (${UNAMESYS},Linux)
        include Makefile-linux
else
	ifeq (${UNAMEPROC},armv7l)
		include Makefile-rpi
	else
		ifeq (${UNAMESYS},linux)
			include Makefile-linux
		else
			ifeq (${UNAMESYS},Darwin)
				include Makefile-darwin
			else
				ifneq (,$(filter CYGWIN_%,${UNAMESYS}))
					include Makefile-windows
				endif
			endif
		endif
	endif
endif

message:
	@echo "To compile for 32-bit Linux:          make -f Makefile-linux bandwidth32"
	@echo "To compile for 64-bit Linux:          make -f Makefile-linux bandwidth64"
	@echo "To compile for 64-bit Mac OS/X:       make -f Makefile-darwin bandwidth64"
	@echo "To compile for Raspberry pi3 32-bit:  make -f Makefile-rpi bandwidth32"
	@echo "To compile for generic ARM Linux:     make -f Makefile-rpi bandwidth-arm32"
	@echo "To compile for 64-bit Windows+Cygwin: make -f Makefile-windows bandwidth64"
	@echo "To compile for 32-bit Windows+Cygwin: make -f Makefile-windows bandwidth32"
	

clean:
	rm -f main.o bandwidth bandwidth32 bandwidth64 routines-x86-32bit.o routines-x86-64bit.o 
	rm -f Testing.o CPUCharacteristics.o
	rm -f bandwidth32.exe bandwidth64.exe
	rm -f utility-x86-32bit.o utility-x86-64bit.o
	rm -f bandwidth.bmp bandwidth32 bandwidth64 
	rm -f bandwidth-arm32 routines-arm-rpi3-32bit.o routines-arm-generic-32bit.o
	rm -rf bandwidth64.dSYM bandwidth32.dSYM
	( cd OOC; make clean )
	rm -f Makefile-flags

OOC32:
	( cd OOC; make OOC32.a )

OOC64:
	( cd OOC; make OOC64.a )

