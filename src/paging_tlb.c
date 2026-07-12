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
    unsigned long lastUsed;  //for LRU
    unsigned long addedTime;  //for MIN
} pageTableEntry;

/*==== TLB implementation ====*/
//TLB entry structure
typedef struct tlb {
    unsigned int vpn;  //VPN
    unsigned int frameNum;  //PFN
    int validBit;  //0 if empty, 1 if filled
    int asid;
    int protectionBits;
    int dirtyBit;
    int referenceBit;
    unsigned long lastUsed;  //for LRU
    unsigned long addedTime;  //for MIN
} tlbEntry;

static void print_header(unsigned int page_size, unsigned int num_vpages, unsigned int num_frames, unsigned int tlb_size, char * strategy)
{
    printf("VM_SIZE\t%uB\n", VM_SIZE);
    printf("RAM_SIZE\t%uB\n", RAM_SIZE);
    printf("PAGE_SIZE\t%uB\n", page_size); // substitute your own vars
    printf("NUM_VPAGES\t%u\n", num_vpages); // substitute your own vars
    printf("NUM_FRAMES\t%u\n", num_frames); // substitute your own vars
    printf("PAGE_POLICY\t%s\n", strategy);
    printf("TLB_SIZE\t%u\n", tlb_size);
    printf("TLB_POLICY\t%s\n\n", strategy);
}

int main(int argc, char *argv[])
{
    // default page size
    unsigned int page_size = 128;

    // default tlb size
    unsigned int tlb_size = 4;

    // default strategy
    char * strategy = "FIFO";

    const char *input_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            page_size = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-t") == 0) {
            tlb_size = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-s") == 0) {
            strategy = argv[i + 1];
            i++;
        } else {
            input_file = argv[i];
        }
    }

    if (page_size <= 0 || page_size > 1024) {
        fprintf(stdout, "Error occurred. Page size is invalid.");
        return EXIT_FAILURE;
    }
    if (tlb_size <= 0) {
        fprintf(stdout, "Error occurred. TLB size is invalid.");
        return EXIT_FAILURE;
    }
    if (strcmp(strategy, "FIFO") != 0 && strcmp(strategy, "LRU") != 0 && strcmp(strategy, "MIN") != 0) {
        fprintf(stdout, "Error occurred. Strategy is invalid.");
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

    print_header(page_size, num_vpages, num_frames, tlb_size, strategy);

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

                    tlb[i].lastUsed = accesses;  //update lastUsed time for LRU logic
                    pageTable[vpn].lastUsed = accesses;

                    //paddr = (frame * page size) + offset
                    //left shift by offsetBits means multiply by 2 power of offsetBits, i.e., page size.
                    //OR operation is addition
                    unsigned int paddr = (frame << offsetBits) | offset;

                    printf("%6d  [vaddr] 0x%04X\t===>\t[paddr] 0x%04X\n", count, vaddr, paddr);
                    count++;

                    break;
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

                    //if RAM is not full yet, give the free frame
                    if (faults <= num_frames) {
                        frame = faults - 1;
                    }
                    else {
                        if (strcmp(strategy, "FIFO") == 0) {
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

                            //change fifoIndex value to the next index
                            fifoIndex++;
                            if (fifoIndex >= num_frames) {  //if the index is out of bound, initialize it to 0
                                fifoIndex = 0;
                            }
                        }
                        else if (strcmp(strategy, "LRU") == 0) {
                            //evict the frame by LRU policy
                            unsigned int evictVpn;  //VPN of the frame to be evicted
                            unsigned long minTime = accesses + 1;  //minimum lastUsed value, initialized by invalid largest value
                            int isEvict = 0;  //if 0, nothing has evicted yet, if 1, something is evicted

                            //by searching the whole page table, find the frame to evict, which is in the RAM and has the minimum lastUsed value
                            for (unsigned int i = 0; i < num_vpages; i++) {
                                if (pageTable[i].presentBit == 1 && pageTable[i].lastUsed < minTime) {
                                    evictVpn = i;
                                    minTime = pageTable[i].lastUsed;
                                    isEvict = 1;
                                }
                            }

                            //evict the found frame
                            if (isEvict == 1) {  //if evictVpn is found and is appropriate
                                frame = pageTable[evictVpn].frameNum;
                                pageTable[evictVpn].presentBit = 0;  //evict the frame

                                //evict it from the TLB too, if it exists
                                for (unsigned int i = 0; i < tlb_size; i++) {
                                    if (tlb[i].validBit == 1 && tlb[i].vpn == evictVpn) {
                                        tlb[i].validBit = 0;
                                    }
                                }
                            }
                        }
                        else if (strcmp(strategy, "MIN") == 0) {
                            //evict the frame by MIN policy
                            unsigned int evictVpn;
                            int maxDistance = -1;  //max distance to re-access
                            int searchSuccess = 0;  // 0 if any of searching is not succeed, 1 if any is succeed
                            unsigned long oldestAdded = accesses + 1;  //will be used when tie
                                                                       //initialize the value as invalid large value to pass first searching value

                            //store location of the file now, before searching the whole file to calculate distance
                            long currentPos = ftell(fp);

                            //calculate re-access distance for all pages
                            for (unsigned int i = 0; i < num_vpages; i++) {
                                if (pageTable[i].presentBit == 1) {
                                    int distance = 0;
                                    int found = 0;
                                    unsigned int fileVaddr;  //store vaddr that will be obtained from the file

                                    //search all vaddr in the file
                                    while (fscanf(fp, "%x", &fileVaddr) == 1) {
                                        distance++;
                                        unsigned int fileVpn = fileVaddr >> offsetBits;  //obtain VPN

                                        //if VPN of the searching vaddr in the file now is the same as
                                        //what we are pointing to in the page table,
                                        //distance calculation is finished
                                        //mark the vaddr as found
                                        if (fileVpn == i) {
                                            found = 1;
                                            break;  //and stop the calculation
                                        }
                                    }

                                    if (found == 0) {  //if VPN is not found until while loop ends, its distance becomes infinity
                                        distance = 100000000;
                                    }

                                    //update values if distance is bigger than current maxDistance OR tie and older than current oldestTime
                                    if (distance > maxDistance || (distance == maxDistance && pageTable[i].addedTime < oldestAdded)) {
                                        maxDistance = distance;
                                        evictVpn = i;
                                        oldestAdded = pageTable[i].addedTime;
                                        searchSuccess = 1;
                                    }

                                    //return to the saved location by ftell in the file to search next page
                                    fseek(fp, currentPos, SEEK_SET);
                                }
                            }

                            //evict the found frame
                            if (searchSuccess == 1) {
                                frame = pageTable[evictVpn].frameNum;
                                pageTable[evictVpn].presentBit = 0;

                                //evict it from the TLB too, if it exists
                                for (unsigned int i = 0; i < tlb_size; i++) {
                                    if (tlb[i].validBit == 1 && tlb[i].vpn == evictVpn) {
                                        tlb[i].validBit = 0;
                                    }
                                }
                            }
                        }
                    }

                    //update the page table with current vaddr
                    pageTable[vpn].presentBit = 1;
                    pageTable[vpn].frameNum = frame;
                    pageTable[vpn].addedTime = accesses;

                    //paddr = (frame * page size) + offset
                    //left shift by offsetBits means multiply by 2 power of offsetBits, i.e., page size.
                    //OR operation is addition
                    unsigned int paddr = (frame << offsetBits) | offset;

                    printf("%6d  [vaddr] 0x%04X\t-x->\t[paddr] 0x%04X\n", count, vaddr, paddr);
                    count++;

                }

                pageTable[vpn].lastUsed = accesses;  //update lastUsed time for LRU logic

                //TLB update
                int done = 0;  //if 1, update is done. if 0, update is not done yet
                //case 1. there is a free space in TLB
                for (unsigned int i = 0; i < tlb_size; i++) {
                    if (tlb[i].validBit == 0) {  //if there is a free space
                        //update the TLB in that space
                        tlb[i].validBit = 1;
                        tlb[i].vpn = vpn;
                        tlb[i].frameNum = frame;
                        tlb[i].addedTime = accesses;
                        tlb[i].lastUsed = accesses;

                        done = 1;  //mark it as done
                        break;
                    }
                }

                //case 2. TLB is full, evict one by chosen strategy
                if (done == 0) {
                    //evict by FIFO policy and replace with new one
                    if (strcmp(strategy, "FIFO") == 0) {
                        tlb[tlbFifoIndex].validBit = 1;
                        tlb[tlbFifoIndex].vpn = vpn;
                        tlb[tlbFifoIndex].frameNum = frame;

                        //change tlbFifoIndex value to the next index
                        tlbFifoIndex++;
                        if (tlbFifoIndex >= tlb_size) {  //if the index is out of bound, initialize it as 0
                            tlbFifoIndex = 0;
                        }
                    }
                    //evict by LRU policy and replace with new one
                    else if (strcmp(strategy, "LRU") == 0) {
                        //pretty much same with pageTable's LRU evict logic
                        int evictIndex = -1;
                        unsigned long minTime = accesses + 1;

                        //find the valid TLB space with the minimum lastUsed time
                        for (unsigned int i = 0; i < tlb_size; i++) {
                            if (tlb[i].validBit == 1 && tlb[i].lastUsed < minTime) {
                                minTime = tlb[i].lastUsed;
                                evictIndex = i;
                            }
                        }

                        //if the space to evict is found and has a valid index, replace the space with the new one
                        if (evictIndex != -1) {
                            tlb[evictIndex].validBit = 1;
                            tlb[evictIndex].vpn = vpn;
                            tlb[evictIndex].frameNum = frame;
                            tlb[evictIndex].lastUsed = accesses;
                        }
                    }
                    //evict by MIN policy and replace with new one
                    else if (strcmp(strategy, "MIN") == 0) {
                        //similar with pageTable's MIN evict logic
                        int evictIndex = -1;
                        int maxDistance = -1;
                        unsigned long oldestAdded = accesses + 1;

                        //store location of the file now, before searching the whole file to calculate distance
                        long currentPos = ftell(fp);

                        //calculate re-access distance for all pages
                        for (unsigned int i = 0; i < tlb_size; i++) {
                            if (tlb[i].validBit == 1) {
                                int distance = 0;
                                int found = 0;
                                unsigned int fileAddr;

                                //search all vaddr in the file
                                while (fscanf(fp, "%x", &fileAddr) == 1) {
                                    distance++;
                                    unsigned int fileVpn = fileAddr >> offsetBits;

                                    //if VPN of the searching vaddr in the file now is the same as
                                    //what we are pointing to in TLB,
                                    //distance calculation is finished
                                    //mark the vaddr as found
                                    if (fileVpn == tlb[i].vpn) {
                                       found = 1;
                                       break;  //and stop the calculation
                                    }
                                }

                                if (found == 0) {  //if VPN is not found until while loop ends, its distance becomes infinity
                                    distance = 100000000;
                                }

                                //update values if distance is bigger than current maxDistance OR tie and older than current oldestTime
                                if (distance > maxDistance || (distance == maxDistance && tlb[i].addedTime < oldestAdded)) {
                                    maxDistance = distance;
                                    evictIndex = i;
                                    oldestAdded = tlb[i].addedTime;
                                }

                                //return to the saved location by ftell in the file to search next page
                                fseek(fp, currentPos, SEEK_SET);
                            }
                        }

                        //if the space to evict is found and has a valid index, replace the space with the new one
                        if (evictIndex != -1) {
                            tlb[evictIndex].validBit = 1;
                            tlb[evictIndex].vpn = vpn;
                            tlb[evictIndex].frameNum = frame;
                            tlb[evictIndex].lastUsed = accesses;

                            tlb[evictIndex].addedTime = accesses;
                        }
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
