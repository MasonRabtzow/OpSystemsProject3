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