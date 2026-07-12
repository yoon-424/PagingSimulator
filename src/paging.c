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


static void print_header(unsigned int page_size, unsigned int num_vpages, unsigned int num_frames)
{
    printf("VM_SIZE\t%uB\n", VM_SIZE);
    printf("RAM_SIZE\t%uB\n", RAM_SIZE);
    printf("PAGE_SIZE\t%uB\n", page_size); // substitute your own vars
    printf("NUM_VPAGES\t%u\n", num_vpages); // substitute your own vars
    printf("NUM_FRAMES\t%u\n", num_frames); // substitute your own vars
    printf("PAGE_POLICY\tFIFO\n\n");
}

int main(int argc, char *argv[])
{
    // default page size
    unsigned int page_size = 128;

    const char *input_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            page_size = atoi(argv[i + 1]);
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

    unsigned long accesses = 0;  //count valid virtual address
    unsigned long faults = 0;  //count page faults

    int count = 0;

    print_header(page_size, num_vpages, num_frames);

    /* ==== for each VADDR ==== */
    while (fscanf(fp, "%x", &vaddr) == 1) {
        //compute VPN and offset
        //extract offset from vaddr by using offsetMask and AND operation
        //extract vpn by removing offset from vaddr by shifting vaddr by offsetBits
        unsigned int offset = vaddr & offsetMask;
        unsigned int vpn = vaddr >> offsetBits;

        if (vpn >= num_vpages) {
            printf ("%6d  [vaddr] 0x%04x\tXXXX\tInvalid vaddr\n", count, vaddr);
            count++;
            continue;
        }

        //check if the index is valid - if not, that vaddr will be ignored
        if (vpn < num_vpages) {

            accesses++;

            unsigned int frame;

            //check if the vaddr exists in page table - if not, evict the frame by FIFO policy and update page table
            if (pageTable[vpn].presentBit == 1) {
                //page hit
                frame = pageTable[vpn].frameNum;

                //paddr = (frame * page size) + offset
                //left shift by offsetBits means multiply by 2 power of offsetBits, i.e., page size.
                //OR operation is addition
                unsigned int paddr = (frame << offsetBits) | offset;

                printf("%6d  [vaddr] 0x%04x\t--->\t[paddr] 0x%04x\n", count, vaddr, paddr);
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

                printf("%6d  [vaddr] 0x%04x\t-x->\t[paddr] 0x%04x\n", count, vaddr, paddr);
                count++;

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

    //free mallocs
    free(pageTable);

    return EXIT_SUCCESS;
}
