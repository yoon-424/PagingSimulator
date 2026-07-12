# PagingSimulator

This program is written in Linux Environment.

## Overview

The project is about implementing basic paging and paging simulator with three different cache replacement policies. 

__What is Paging?__

Paging is the solution for external fragmentation, working by divide physical memory and virtual memory into frames/pages of equally sized chunks for each. 
With page table which locations and properties of all pages and some calculations, we can obtain virtual address and physical address of each page. 
Page fault occurs when page is not present in physical memory. Opposite, page hit occurs when page is present in physical memory. 

__What is TLB?__

TLB, translation lookaside buffer, is a memory cache for storing translations of virtual memory to physical memory address. It helps program to be fast and reduce memory accesses by reducing chances to look up all the pages in page table. 
If the virtual address that the program is looking for is in the TLB, page table lookup can be avoided(it is called TLB hit). If not, then page table should be searched. 
Since TLB is small, some page should be removed when the cache is full. There are some options in this 'page replacement policy'.
This program has three page replacement policy options. 

__Page Replacement Options__
+ __FIFO (First In, First Out)__
   - Items are added in the order of arrival.
   - Item that has been in the cache the longest time(first in) is the first to be evicted.

+ __LRU (Least Recently Used)__
   - Item that has not been used for the logest time(least recently used) is the first to be evicted.

+ __MIN (optimal)__
   - Item that will be accessed furthest in the future is the first to be evicted.
   - It is called optimal because it has fewest possible cache misses.
   - To implement this policy, all virtual pages including pages will be used in the future should be accessed. 

## Installation

__How to Compile on Linux__

There is a makefile for simple compilation and execution of the program. The makefile has the targets of:
+ __make paging__ : make the executable file for basic paging simulator called paging
+ __make paging_tlb_fifo__ : make the executable file for paging simulator with FIFO page replacement policy called paging_tlb_fifo
+ __make paging_tlb__ : make the executable file for paging simulator with three page replacement policy options, FIFO, LRU, and MIN, called paging_tlb
+ __make all__ : make all executable files, paging, paging_tlb_fifo, and paging_tlb
+ __make clean__ : delete all the executable files
+ __make debug__ : delete all the executable files and compile the source files with debugging options

## Usage

Basic paging simulator will be run with the command: __./paging [-p page_size] <input.txt>__

Paging simulator with FIFO policy will be run with the command: __./paging_tlb_fifo [-p page_size] [-t tlb_size] <input.txt>__

Paging simulator with FIFO, LRU, and MIN policy options will be run with the command: __./paging_tlb [-p page_size] [-t tlb_size] [-s strategy] <input.txt>__
+ In the command, __<input.txt>__ is mandatory, and __[arguments]__ are optional. 
+ In __<input.txt>__, put the text file with virtual addresses in hex format for each line. 
+ In __[-p page_size]__, put the page size in bytes. 128 bytes in default. 
+ In __[-t tlb_size]__, put the TLB size in number of entries. 4 entries in default. 
+ In __[-s strategy]__, put the cache replacement strategy(FIFO, LRU, or MIN). It will work for both page faults and TLB misses. FIFO in default. 

If file reading is failed for some reasons, the program prints error message and exits. 

If page size is less than 1 or bigger than 1024, the program will print error message and exit.

If TLB size is less than 1, the program will print error message and exit.

If strategy options is other than FIFO, LRU, and MIN, the program will print error message and exit. 

### Sample Inputs & Outputs

__command: ./paging test01.txt__

input: test01.txt
<pre>
<code>
0005
0105
0205
0305
001A
011A
021A
031A
</code>
</pre>

output: virtual to physical address translation results on console
<pre>
<code>
VM_SIZE 4096B
RAM_SIZE        1024B
PAGE_SIZE       128B
NUM_VPAGES      32
NUM_FRAMES      8
PAGE_POLICY     FIFO

     0  [vaddr] 0x0005  -x->    [paddr] 0x0005
     1  [vaddr] 0x0105  -x->    [paddr] 0x0085
     2  [vaddr] 0x0205  -x->    [paddr] 0x0105
     3  [vaddr] 0x0305  -x->    [paddr] 0x0185
     4  [vaddr] 0x001a  --->    [paddr] 0x001a
     5  [vaddr] 0x011a  --->    [paddr] 0x009a
     6  [vaddr] 0x021a  --->    [paddr] 0x011a
     7  [vaddr] 0x031a  --->    [paddr] 0x019a

Total accesses : 8
Page faults    : 4
Fault rate     : 50.00%
</code>
</pre>