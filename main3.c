#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define DEFAULT_FRAME_COUNt 128
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
// For LRU, use a counter to track the last access time of each frame
static int last_used[MAX_FRAME_COUNT];

static int frame_count;
static int next_free_frame;
static int tlb_fifo_index;
static int page_faults;
static int tlb_hits;
static int total_addresses;
static int access_clock;

static void init_system(int frames) {
    int i;
    frame_count = frames;
    next_free_frame = 0;
    tlb_fifo_index = 0;
    page_faults = 0;
    tlb_hits = 0;
    total_addresses = 0;
    access_clock = 0;

    for (i = 0; i < PAGE_TABLE_SIZE; i++) page_table[i] = INVALID;
    for (i = 0; i < frame_count; i++) {
        frame_to_page[i] = INVALID;
        last_used[i] = -1;
    }
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

static void invalidate_tlb_page(int page) {
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].page == page) {
            tlb[i].page = INVALID;
            tlb[i].frame = INVALID;
        }
    }
}

static void add_to_tlb(int page, int frame) {
    tlb[tlb_fifo_index].page = page;
    tlb[tlb_fifo_index].frame = frame;
    tlb_fifo_index = (tlb_fifo_index + 1) % TLB_SIZE;
}

//choose lru

static int load_page(FILE *backing_store, int page) {
    int frame;
    long offset = (long) page * PAGE_SIZE;

    if (next_free_frame < frame_count) {
        frame = next_free_frame;
        next_free_frame++;
    } else {
        int victim_page;
        frame = choose_lru_frame();
        victim_page = frame_to_page[frame];
        if (victim_page != INVALID) {
            page_table[victim_page] = INVALID;
            invalidate_tlb_page(victim_page);
        }
    }

    if (fseek(backing_store, offset, SEEK_SET) != 0) {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    if (fread(&physical_memory[frame * PAGE_SIZE], sizeof(signed char), PAGE_SIZE, backing_store) != PAGE_SIZE) {
        perror("fread");
        exit(EXIT_FAILURE);
    }

    page_table[page] = frame;
    frame_to_page[frame] = page;
    last_used[frame] = access_clock;
    page_faults++;
    return frame;
}

