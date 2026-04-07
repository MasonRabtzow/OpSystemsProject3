#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Constants for system sizes
#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define DEFAULT_FRAME_COUNT 128
#define MAX_FRAME_COUNT 1024
#define INVALID -1

//TLB entry stores page to frame mapping
struct TLBEntry {
    int logical_page;
    int frame_number;
    int valid;
};

//Physical memory & mappings
static signed char physical_memory[MAX_FRAME_COUNT * PAGE_SIZE];
static int page_table[PAGE_TABLE_SIZE];
static int frame_to_page[MAX_FRAME_COUNT];
static TLBEntry tlb[TLB_SIZE];

// Global state/stats
static int frame_count;
static int next_free_frame; //fills memory
static int next_victim_frame; //FIFO replacement
static int tlb_fifo_index;
static int page_faults;
static int tlb_hits;
static int total_addresses;

//Initialize
static void init_system(int frames) {
    int i;
    frame_count = frames;
    next_free_frame = 0;
    next_victim_frame = 0;
    tlb_fifo_index = 0;
    page_faults = 0;
    tlb_hits = 0;
    total_addresses = 0;

    //Mark invalid
    for (i = 0; i < PAGE_TABLE_SIZE; i++) page_table[i] = INVALID;
    for (i = 0; i < frame_count; i++) frame_to_page[i] = INVALID;
    for (i = 0; i < TLB_SIZE; i++) {
        tlb[i].page = INVALID;
        tlb[i].frame = INVALID;
    }
}

//Linear search TLB
static int search_tlb(int page) {
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].page == page) return tlb[i].frame;
    }
    return INVALID;
}

// Remove a page from TLB
static void invalidate_tlb_page(int page) {
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].page == page) {
            tlb[i].page = INVALID;
            tlb[i].frame = INVALID;
        }
    }
}

// Add to TLB using FIFO
static void add_to_tlb(int page, int frame) {
    tlb[tlb_fifo_index].page = page;
    tlb[tlb_fifo_index].frame = frame;
    tlb_fifo_index = (tlb_fifo_index + 1) % TLB_SIZE;
}