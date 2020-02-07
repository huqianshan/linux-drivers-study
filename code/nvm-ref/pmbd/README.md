#### NOTE: PMDB was an Intel research project and is not currently being maintained.  For current information and instructions related to persistent memory enabling in Linux please refer to https://nvdimm.wiki.kernel.org/ ####

===============================================================================
              INTEL PERSISTENT MEMORY BLOCK DRIVER (PMBD) v0.9
===============================================================================

This software implements a block device driver for persistent memory (PM).
This module provides a block-based logical interface to manage PM that is
physically attached to the system memory bus. 

The architecture is assumed as follows. Both DRAM and PM DIMMs are directly
attached to the host memory bus. The PM space is presented to the operating
system as a contiguous range of physical memory address space at the high end. 

There are three major design considerations: (1) Data protection - Private
mapping is used to prevent stray pointers (in kernel/driver bugs) to
accidentally wipe off persistent PM data. (2) Data persistence - Non-temporal
store and fence instructions are used to leverage the processor store buffer
and avoid polluting the CPU cache. (3) Write ordering - Write barrier is
supported to ensure correct order of writes. 

This module also includes other (experimental) features, such as PM speed
emulation, checksum for page integrity, partial page updates, write
verification, etc. Please refer to the help page of the module. 


===============================================================================
                 COMPILING AND INSTALLING THE PMBD DRIVER
===============================================================================

1. Compile the PMBD driver:

   $ make

2. Install the PMBD driver:

   $ sudo make install

3. Check available driver information:

   $ modinfo pmbd

NOTE: This module runs with Linux kernel 3.2.1 or 2.6.34. For other kernel
versions, please search for "KERNEL_VERSION" and change the code as needed.

===============================================================================
                  QUICK USER'S GUIDE OF THE PMBD DRIVER
===============================================================================

1. modify /etc/grub.conf to set the physical memory address range that
   is to be simulated as PM. 

   Add the following to the boot option line:

        memmap=<PM_SIZE_GB>G$<DRAM_SIZE_GB>G numa=off 

   NOTE: 

   PM_SIZE_GB - the PM space size (in GBs)
   DRAM_SIZE_GB - the DRAM space size (in GBs)

   Example: 

   Assuming a total memory capacity of 24GB, and if we want to use 16GB PM and
   8GB DRAM, it should be "memmap=16G$8G". 
   
2. Reboot and check if the memory size is set as expected. 
   
   $ sudo reboot; exit
   $ free

3. Load the device driver module

   Load the driver module into the kernel with private mapping, non-temp store,
   and write barrier enabled (*** RECOMMENDED CONFIG ***):

   $ modprobe pmbd mode="pmbd<PM_SIZE_GB>;hmo=<DRAM_SIZE_GB>;hms<PM_SIZE_GB>; \
                        pmapY;ntsY;wbY;"

   Check the kernel message output:

   $ dmesg 
   
   After loading the module, a block device (/dev/pma) should appear. Since
   now, it can be used as any block device, such as fdisk, mkfs, etc. 

4. Unload the device driver

   $ rmmod pmbd

===============================================================================
  OTHER CONFIGURATION OPTIONS OF THE PERSISTENT MEMORY DEVICE DRIVER MODULE
===============================================================================

usage: $ modprobe pmbd mode="pmbd<#>;hmo<#>;hms<#>;[Option1];[Option2];;.."

GENERAL OPTIONS:
 pmbd<#,#..>     set pmbd size (GBs)
 HM|VM           use high memory (HM default) or vmalloc (VM)
 hmo<#>          high memory starting offset (GB)
 hms<#>          high memory size (GBs)
 pmap<Y|N>       use private mapping (Y) or not (N default) - (note: must
                 enable HM and wrprotN)
 nts<Y|N>        use non-temporal store (MOVNTQ) and sfence to do memcpy (Y), 
                 or regular memcpy (N default)
 wb<Y|N>         use write barrier (Y) or not (N default)
 fua<Y|N>        use WRITE_FUA (Y default) or not (N) (only effective for
                 Linux 3.2.1)
 ntl<Y|N>        use non-temporal load (MOVNTDQA) to do memcpy (Y), or
                 regular memcpy (N default) - this option enforces memory type 
                 of write combining


SIMULATION:
 simmode<#,#..>  use the specified numbers to the whole device (0 default) or
                 PM only (1)
 rdlat<#,#..>    set read access latency (ns)
 wrlat<#,#..>    set write access latency (ns)
 rdbw<#,#..>     set read bandwidth (MB/sec)  (if set 0, no emulation)
 wrbw<#,#..>     set write bandwidth (MB/sec) (if set 0, no emulation)
 rdsx<#,#..>     set the relative slowdown (x) for read
 wrsx<#,#..>     set the relative slowdown (x) for write
 rdpause<#,.>    set a pause (cycles per 4KB) for each read
 wrpause<#,.>    set a pause (cycles per 4KB) for each write
 adj<#>          set an adjustment to the system overhead (nanoseconds)

WRITE PROTECTION:
 wrprot<Y|N>     use write protection for PM pages? (Y or N)
 wpmode<#,#,..>  write protection mode: use the PTE change (0 default) or flip
                 CR0/WP bit (1)
 clflush<Y|N>    use clflush to flush CPU cache for each write to PM space?
                 (Y or N)
 wrverify<Y|N>   use write verification for PM pages? (Y or N)
 checksum<Y|N>   use checksum to protect PM pages? (Y or N)
 bufsize<#,#,..> the buffer size (MBs) (0 - no buffer, at least 4MB)
 bufnum<#>       the number of buffers for a PMBD device (16 buffers, at least 1
                 if using buffer, 0 -no buffer)
 bufstride<#>    the number of contiguous blocks(4KB) mapped into one buffer
                 (bucket size for round-robin mapping) (1024 in default)
 batch<#,#>      the batch size (num of pages) for flushing PMBD buffer (1 means
                 no batching)

MISC:
 mgb<Y|N>        mergeable? (Y or N)
 lock<Y|N>       lock the on-access page to serialize accesses? (Y or N)
 cache<WB|WC|UC> use which CPU cache policy? Write back (WB), Write Combined
                 (WB), or Uncachable (UC)
 subupdate<Y|N>  only update the changed cachelines of a page? (Y or N) (check
                 PMBD_CACHELINE_SIZE)
 timestat<Y|N>   enable the detailed timing statistics (/proc/pmbd/pmbdstat)?
                 This will cause significant performance slowdown (Y or N)

NOTE:
 (1) Option rdlat/wrlat only specifies the minimum access times. Real access
     times can be higher.
 (2) If rdsx/wrsx is specified, the rdlat/wrlat/rdbw/wrbw would be ignored.
 (3) Option simmode1 applies the simulated specification to the PM space,
     rather than the whole device, which may have buffer.

WARNING:
 (1) When using simmode1 to simulate slow-speed PM space, soft lockup warning
     may appear. Use "nosoftlockup" boot option to disable it.  
 (2) Enabling timestat may cause performance degradation.
 (3) FUA is supported in Linux 3.2.1, but if buffer is used (for PT based
     protection), enabling FUA lowers performance due to double writes.
 (4) No support for changing CPU cache related PTE attributes for VM-based PMBD
     in Linux 3.2.1 (RCU stalls).

PROC ENTRIES:
 /proc/pmbd/pmbdcfg:   config info about the PMBD devices
 /proc/pmbd/pmbdstat:  statistics of the PMBD devices (if timestat is enabled)

EXAMPLE:
 Assuming a 16GB PM space with physical memory addresses from 8GB to 24GB:
 (1) Basic (Ramdisk): 
     $ sudo modprobe pmbd mode="pmbd16;hmo8;hms16;"

 (2) Protected (with private mapping): 
     $ sudo modprobe pmbd mode="pmbd16;hmo8;hms16;pmapY;"

 (3) Protected and synced (with private mapping, non-temp store): 
     $ sudo modprobe pmbd mode="pmbd16;hmo8;hms16;pmapY;ntsY;"

 (4) *** RECOMMENDED CONFIGURATION ***
     Protected, synced, and ordered (with private mapping, nt-store, write
     barrier): 
     $ sudo modprobe pmbd mode="pmbd16;hmo8;hms16;pmapY;ntsY;wbY;"




