KVERS = $(shell uname -r)

# Kernel modules
obj-m += tdlcd_fifo.o

# Specify flags for the module compilation.
EXTRA_CFLAGS=-g -O0

build: kernel_modules test aio

kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm -f tdlcd_epoll_test tdlcd_poll_test tdlcd_async_test aio
test:
	gcc -o tdlcd_epoll_test -O tdlcd_fifo_epoll_test.c
	gcc -o tdlcd_poll_test -O tdlcd_fifo_poll_test.c
	gcc -o tdlcd_async_test -O tdlcd_fifo_async_test.c
aio: aior.c
	gcc -o aior -O aior.c -laio
