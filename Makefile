# DO NOT MODIFY MAKEFILE

CC = gcc
CFLAGS = -Wall -Wextra
DEBUG_FLAGS = -g -DDEBUG

SRC = src

all: paging paging_tlb_fifo paging_tlb

paging: $(SRC)/paging.c $(SRC)/paging.h
	$(CC) $(CFLAGS) $(SRC)/paging.c -o paging

paging_tlb_fifo: $(SRC)/paging_tlb_fifo.c $(SRC)/paging_tlb_fifo.h
	$(CC) $(CFLAGS) $(SRC)/paging_tlb_fifo.c -o paging_tlb_fifo

paging_tlb: $(SRC)/paging_tlb.c $(SRC)/paging_tlb.h
	$(CC) $(CFLAGS) $(SRC)/paging_tlb.c -o paging_tlb

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) $(DEBUG_FLAGS)" all

clean:
	rm -f paging paging_tlb_fifo paging_tlb

.PHONY: all clean debug
