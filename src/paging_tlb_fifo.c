#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define VM_SIZE  (4 * 1024)   // 4KB virtual memory
#define RAM_SIZE (1 * 1024)   // 1KB RAM

/*==== page table implementation ====*/
//array index is the vpage number(VPN)
//table structure
typedef struct pageTable {
    int frameNum;  //PFN
    int validBit;  //marks invalid addresses in the virtual address space
    int presentBit;  //if this page is in the physical memory
    int dirtyBit;  //has this page been modified
    int referenceBit;  //has this page been accessed
    int permission;  //read-only or writable
} pageTableEntry;

/*==== TLB implementation ====*/
//TLB entry structure
typedef struct tlb {
    unsigned int vpn;  //VPN
    unsigned int frameNum;  //PFN
    int validBit;  //0 if empty, 1 if filled
    //int asid
    //int protectionBits;
    //int dirtyBit;
    //int referenceBit;
} tlbEntry;

static void print_header(unsigned int page_size, unsigned int num_vpages, unsigned int num_frames, unsigned int tlb_size)
{
    printf("VM_SIZE\t%uB\n", VM_SIZE);
    printf("RAM_SIZE\t%uB\n", RAM_SIZE);
    printf("PAGE_SIZE\t%uB\n", page_size); // substitute your own vars
    printf("NUM_VPAGES\t%u\n", num_vpages); // substitute your own vars
    printf("NUM_FRAMES\t%u\n", num_frames); // substitute your own vars
    printf("PAGE_POLICY\tFIFO\n");
    printf("TLB_SIZE\t%u\n", tlb_size);
    printf("TLB_POLICY\tFIFO\n\n");
}

int main(int argc, char *argv[])
{
    // default page size
    unsigned int page_size = 128;

    // default tlb size
    unsigned int tlb_size = 4;

    const char *input_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            page_size = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-t") == 0) {
            tlb_size = atoi(argv[i + 1]);
            i++;
        } else {
            input_file = argv[i];
        }
    }

    if (page_size <= 0 || page_size > 1024) {
        fprintf(stdout, "Error occurred. Page size is invalid.");
        return EXIT_FAILURE;
    }

    //required variables
    unsigned int num_vpages = VM_SIZE / page_size;
    unsigned int num_frames = RAM_SIZE / page_size;

    //set the page table and initialize it
    pageTableEntry *pageTable = (pageTableEntry *) malloc (num_vpages * sizeof(pageTableEntry));
    for (unsigned int i = 0; i < num_vpages; i++) {
        pageTable[i].presentBit = 0;  //show that the page is not in RAM
        pageTable[i].frameNum = -1;  //PFN does not exist yet
    }

    //set the TLB and initialize it
    tlbEntry *tlb = (tlbEntry *) malloc (tlb_size * sizeof(tlbEntry));
    for (unsigned int i = 0; i < tlb_size; i++) {
        tlb[i].validBit = 0;
    }

    //open the file
    FILE *fp = fopen(input_file, "r");
    if (fp == NULL) {
        perror("Error occurred. File cannot be opened.");
        return EXIT_FAILURE;
    }

    unsigned int vaddr;
    unsigned int offsetMask = page_size - 1;  //variable to mask offset

    unsigned int offsetBits = 0;  //variable of number of bits in offset - will be used when extract VPN
                                  //can be obtained by log_2(page_size), i.e. the number of page size can be divided by 2
    unsigned int temp = page_size;
    while (temp > 1) {
        temp = temp >> 1;  //right shift by 1 is the same as division by 2
        offsetBits++;
    }

    unsigned int fifoIndex = 0;
    unsigned int tlbFifoIndex = 0;

    unsigned long accesses = 0;  //count valid virtual address
    unsigned long faults = 0;  //count page faults

    unsigned long tlbHits = 0;  //count TLB hit
    unsigned long tlbMisses = 0;  //count TLB miss

    int count = 0;

    print_header(page_size, num_vpages, num_frames, tlb_size);

    /* ==== for each VADDR ==== */
    while (fscanf(fp, "%x", &vaddr) == 1) {
        //compute VPN and offset
        //extract offset from vaddr by using offsetMask and AND operation
        //extract vpn by removing offset from vaddr by shifting vaddr by offsetBits
        unsigned int offset = vaddr & offsetMask;
        unsigned int vpn = vaddr >> offsetBits;

        if (vpn >= num_vpages) {
            printf ("%6d  [vaddr] 0x%04X\tXXXX\tInvalid vaddr\n", count, vaddr);
            count++;
            continue;
        }

        //check if the index is valid - if not, that vaddr will be ignored
        if (vpn < num_vpages) {

            accesses++;

            int isTlbHit = 0;  //if 1, TLB hit. if 0, TLB miss
            unsigned int frame;

            //check if TLB hits
            for (unsigned int i = 0; i < tlb_size; i++) {
                if (tlb[i].validBit == 1 && tlb[i].vpn == vpn) {  //TLB hit
                    frame = tlb[i].frameNum;  //return frame right away from TLB
                    isTlbHit = 1;
                    tlbHits++;

                    //paddr = (frame * page size) + offset
                    //left shift by offsetBits means multiply by 2 power of offsetBits, i.e., page size.
                    //OR operation is addition
                    unsigned int paddr = (frame << offsetBits) | offset;

                    printf("%6d  [vaddr] 0x%04X\t===>\t[paddr] 0x%04X\n", count, vaddr, paddr);
                    count++;

                    break;
                    //check page hit or page fault - if page fault, increment faults count
                    //if (pageTable[vpn].presentBit != 1) {
                        //faults++;
                    //}
                }
            }

            if (isTlbHit == 0) {  //if TLB miss, search page table
                tlbMisses++;

                //check if the vaddr exists in page table - if not, evict the frame by FIFO policy and update page table
                if (pageTable[vpn].presentBit == 1) {

                    //page hit
                    frame = pageTable[vpn].frameNum;

                    //paddr = (frame * page size) + offset
                    //left shift by offsetBits means multiply by 2 power of offsetBits, i.e., page size.
                    //OR operation is addition
                    unsigned int paddr = (frame << offsetBits) | offset;

                    printf("%6d  [vaddr] 0x%04X\t--->\t[paddr] 0x%04X\n", count, vaddr, paddr);
                    count++;
                } else {
                    //page fault
                    faults++;

                    frame = fifoIndex;

                    //evict the frame by FIFO policy
                    for (unsigned int i = 0; i < num_vpages; i++) {
                        //evict the frame that is in the RAM and fifoIndex(frame) points to
                        if (pageTable[i].presentBit == 1 && pageTable[i].frameNum == (int)frame) {
                            pageTable[i].presentBit = 0;

                            //evict it from the TLB too, if it exists
                            for (unsigned int j = 0; j < tlb_size; j++) {
                                if (tlb[j].validBit == 1 && tlb[j].vpn == i) {
                                    tlb[j].validBit = 0;
                                }
                            }
                            break;
                        }
                    }

                    //update the page table with current vaddr
                    pageTable[vpn].presentBit = 1;
                    pageTable[vpn].frameNum = frame;

                    //change fifoIndex value to the next index
                    fifoIndex++;
                    if (fifoIndex >= num_frames) {  //if the index is out of bound, initialize it to 0
                        fifoIndex = 0;
                    }

                    //paddr = (frame * page size) + offset
                    //left shift by offsetBits means multiply by 2 power of offsetBits, i.e., page size.
                    //OR operation is addition
                    unsigned int paddr = (frame << offsetBits) | offset;

                    printf("%6d  [vaddr] 0x%04X\t-x->\t[paddr] 0x%04X\n", count, vaddr, paddr);
                    count++;

                }

                //TLB update
                int done = 0;  //if 1, update is done. if 0, update is not done yet
                //case 1. there is a free space in TLB
                for (unsigned int i = 0; i < tlb_size; i++) {
                    if (tlb[i].validBit == 0) {  //if there is a free space
                        //update the TLB in that space
                        tlb[i].validBit = 1;
                        tlb[i].vpn = vpn;
                        tlb[i].frameNum = frame;

                        done = 1;  //mark it as done
                        break;
                    }
                }

                //case 2. TLB is full, evict one by FIFO policy
                if (done == 0) {
                    tlb[tlbFifoIndex].validBit = 1;
                    tlb[tlbFifoIndex].vpn = vpn;
                    tlb[tlbFifoIndex].frameNum = frame;

                    //change tlbFifoIndex value to the next index
                    tlbFifoIndex++;
                    if (tlbFifoIndex >= tlb_size) {  //if the index is out of bound, initialize it as 0
                        tlbFifoIndex = 0;
                    }
                }
            }
        }
    }

    printf("\nTotal accesses : %lu\n", accesses);
    printf("Page faults    : %lu\n", faults);
    if (accesses > 0) {
        double fault_rate = 100.0 * faults / accesses;
        printf("Fault rate     : %.2f%%\n", fault_rate);
    } else {
        printf("Fault rate     : N/A (no accesses)\n");
    }
    printf("TLB hits       : %lu\n", tlbHits);
    printf("TLB misses     : %lu\n", tlbMisses);
    if (accesses > 0) {
        double hitRate = 100.0 * tlbHits / accesses;
        printf("TLB hit rate   : %.2f%%\n", hitRate);
    } else {
        printf("TLB hit rate   : N/A (no accesses)\n");
    }

    //free mallocs
    free(pageTable);

    return EXIT_SUCCESS;
}
