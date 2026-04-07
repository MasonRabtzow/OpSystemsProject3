#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define DEFAULT_FRAME_COUNT 128
#define MAX_FRAME_COUNT 1024
#define INVALID -1

typedef struct {
    int page;
    int frame;
} TLBEntry;

static signed char physical_memory[MAX_FRAME_COUNT * PAGE_SIZE];
static int page_table[PAGE_TABLE_SIZE];
static int frame_to_page[MAX_FRAME_COUNT];
static TLBEntry tlb[TLB_SIZE];

static int frame_count;
static int next_free_frame;
static int next_victim_frame;
static int tlb_fifo_index;
static int page_faults;
static int tlb_hits;
static int total_addresses;

static void init_system(int frames) {
    int i;
    frame_count = frames;
    next_free_frame = 0;
    next_victim_frame = 0;
    tlb_fifo_index = 0;
    page_faults = 0;
    tlb_hits = 0;
    total_addresses = 0;

    for (i = 0; i < PAGE_TABLE_SIZE; i++) page_table[i] = INVALID;
    for (i = 0; i < frame_count; i++) frame_to_page[i] = INVALID;
    for (i = 0; i < TLB_SIZE; i++) {
        tlb[i].page = INVALID;
        tlb[i].frame = INVALID;
    }
}

static int search_tlb(int page) {
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].page == page) return tlb[i].frame;
    }
    return INVALID;
}