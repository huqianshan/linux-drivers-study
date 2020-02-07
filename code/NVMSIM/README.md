# A Block driver of Non-volatile Memory

## Version 1.0

### File Contains

- `ramdecive.h/c` the implementention of driver

- `mem.c` the implementention of  `memcpy` like function

- `nvmconfig.h` contains all of `#define` configuration (Current Not Used)

### Architecture

#### The simulation of `Non-volatile Memory`

- Write Delay

- Write Bandwidth

#### The Block Driver For `NVM`

- The Memory Management
  - Highmemory and Mapping to Kernel
  - `DONE`

- Write/Read Function
  - The Test of I/O throughput
  - `DONE`

#### Free Block Manaement

- `BitMap`


#### The Address Transformer Mechanism

#### The Access Information Summary Table

##### Page: 32 Sectors (1 int size)

- Entry: Logic Sectors Number(23bits) Value: int 9 bits,equal to 1 int


#### Data consistency and Fault-Tolerance Mechaism
