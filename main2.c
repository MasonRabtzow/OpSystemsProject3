#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define DEFAULT_FRAME_COUNT 128
#define MAX_FRAME_COUNT 1024
#define INVALID -1

// TLB entry stores page to frame mapping
typedef struct TLBEntry {
    int page;
    int frame;
    int valid;
} TLBEntry;

// Physical memory & mappings
static signed char physical_memory[MAX_FRAME_COUNT * PAGE_SIZE];
static int page_table[PAGE_TABLE_SIZE];
static int frame_to_page[MAX_FRAME_COUNT];
static TLBEntry tlb[TLB_SIZE];

// Global state/stats
static int frame_count;
static int next_free_frame;     // fills memory
static int next_victim_frame;   // FIFO replacement
static int tlb_fifo_index;
static int page_faults;
static int tlb_hits;
static int total_addresses;

// Initialize
static void init_system(int frames) {
    int i;
    frame_count = frames;
    next_free_frame = 0;
    next_victim_frame = 0;
    tlb_fifo_index = 0;
    page_faults = 0;
    tlb_hits = 0;
    total_addresses = 0;

    // Mark invalid
    for (i = 0; i < PAGE_TABLE_SIZE; i++) page_table[i] = INVALID;
    for (i = 0; i < frame_count; i++) frame_to_page[i] = INVALID;
    for (i = 0; i < TLB_SIZE; i++) {
        tlb[i].page = INVALID;
        tlb[i].frame = INVALID;
        tlb[i].valid = 0;
    }
}

// Linear search TLB
static int search_tlb(int page) {
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page == page) {
            return tlb[i].frame;
        }
    }
    return INVALID;
}

// Remove a page from TLB
static void invalidate_tlb_page(int page) {
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page == page) {
            tlb[i].page = INVALID;
            tlb[i].frame = INVALID;
            tlb[i].valid = 0;
        }
    }
}

// Add to TLB using FIFO
static void add_to_tlb(int page, int frame) {
    tlb[tlb_fifo_index].page = page;
    tlb[tlb_fifo_index].frame = frame;
    tlb[tlb_fifo_index].valid = 1;
    tlb_fifo_index = (tlb_fifo_index + 1) % TLB_SIZE;
}

// Load page from backing store into memory
static int load_page(FILE *backing_store, int page) {
    int frame;
    long offset = (long) page * PAGE_SIZE;

    // If space available -> use free frame
    if (next_free_frame < frame_count) {
        frame = next_free_frame;
        next_free_frame++;
    }
    // Otherwise -> replace using FIFO
    else {
        int victim_page;
        frame = next_victim_frame;
        next_victim_frame = (next_victim_frame + 1) % frame_count;

        // invalidate old mappings
        victim_page = frame_to_page[frame];
        if (victim_page != INVALID) {
            page_table[victim_page] = INVALID;
            invalidate_tlb_page(victim_page);
        }
    }

    // Read page from backing store into memory
    if (fseek(backing_store, offset, SEEK_SET) != 0) {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    if (fread(&physical_memory[frame * PAGE_SIZE], sizeof(signed char), PAGE_SIZE, backing_store) != PAGE_SIZE) {
        perror("fread");
        exit(EXIT_FAILURE);
    }

    // Update mappings
    page_table[page] = frame;
    frame_to_page[frame] = page;
    page_faults++;
    return frame;
}

int main(int argc, char *argv[]) {
    FILE *address_file;
    FILE *backing_store;
    FILE *out1;
    FILE *out2;
    FILE *out3;
    int logical_address;
    int frames = DEFAULT_FRAME_COUNT;

    // Check args
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s addresses.txt [frame_count]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Optional frame count input
    if (argc == 3) {
        frames = atoi(argv[2]);
        if (frames <= 0 || frames > MAX_FRAME_COUNT) {
            fprintf(stderr, "frame_count must be between 1 and %d\n", MAX_FRAME_COUNT);
            return EXIT_FAILURE;
        }
    }

    // Open input + backing store
    address_file = fopen(argv[1], "r");
    if (address_file == NULL) {
        perror("addresses file");
        return EXIT_FAILURE;
    }

    backing_store = fopen("BACKING_STORE.bin", "rb");
    if (backing_store == NULL) {
        perror("BACKING_STORE.bin");
        fclose(address_file);
        return EXIT_FAILURE;
    }

    // Output files
    out1 = fopen("out1.txt", "w");
    out2 = fopen("out2.txt", "w");
    out3 = fopen("out3.txt", "w");

    init_system(frames);

    // Main translation loop
    while (fscanf(address_file, "%d", &logical_address) == 1) {
        int masked_address = logical_address & 0xFFFF;
        int page = (masked_address >> 8) & 0xFF;
        int offset = masked_address & 0xFF;

        int frame = search_tlb(page);
        int physical_address;
        int value;

        total_addresses++;

        if (frame != INVALID) {
            tlb_hits++;
        } else {
            frame = page_table[page];

            // page fault if not in memory
            if (frame == INVALID) {
                frame = load_page(backing_store, page);
            }

            add_to_tlb(page, frame);
        }

        // compute final physical address
        physical_address = (frame * PAGE_SIZE) + offset;
        value = physical_memory[physical_address];

        // write outputs
        fprintf(out1, "%d\n", masked_address);
        fprintf(out2, "%d\n", physical_address);
        fprintf(out3, "%d\n", value);
    }

    // Print stats
    printf("Frame Count = %d\n", frames);
    printf("Number of Translated Addresses = %d\n", total_addresses);
    printf("Page Faults = %d\n", page_faults);
    printf("Page Fault Rate = %.3f\n", total_addresses ? (double) page_faults / total_addresses : 0.0);
    printf("TLB Hits = %d\n", tlb_hits);
    printf("TLB Hit Rate = %.3f\n", total_addresses ? (double) tlb_hits / total_addresses : 0.0);

    // Cleanup
    fclose(address_file);
    fclose(backing_store);
    fclose(out1);
    fclose(out2);
    fclose(out3);

    return EXIT_SUCCESS;
}