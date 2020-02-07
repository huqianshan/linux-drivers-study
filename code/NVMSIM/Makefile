
obj-m := nvmsim.o

nvmsim-objs += ramdevice.o mem.o


CC = gcc
FLAGS = -O -Wall 
LIBS = -lm
# From: linuxdevcenter.com

# KDIR is the location of the kernel source.  The current standard is
# to link to the associated source tree from the directory containing
# the compiled modules.
#KDIR  := /lib/modules/$(shell uname -r)/build

# PWD is the current working directory and the location of our module
# source files.
#PWD   := $(shell pwd)

# default is the default make target.  The rule here says to run make
# with a working directory of the directory containing the kernel
# source and compile only the modules in the PWD (local) directory.

build: nvmsim test

nvmsim:
	make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules
#default:
#	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.c Module.* modules.* \
	       *~ .*~ .\#*~ \#*~ .*.cmd .tmp* bitmap

test: bit_map.c bit_map.h
	${CC} ${FLAGS} -o bitmap bit_map.c ${LIBS}
