//main3.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define DEFAULT_FRAME_COUNT 128
#define INVALID -1

typedef struct {
    int page;
    int frame;
} TLBEntry;

static signed char physical_memory[DEFAULT_FRAME_COUNT * PAGE_SIZE];
static int page_table[PAGE_TABLE_SIZE];
static int frame_to_page[DEFAULT_FRAME_COUNT];
static TLBEntry tlb[TLB_SIZE];
// For LRU, use a counter to track the last access time of each frame
static int last_used[DEFAULT_FRAME_COUNT];

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
static int choose_lru_frame(void) {
    int i;
    int victim = 0;
    int oldest = last_used[0];
    for (i = 1; i < frame_count; i++) {
        if (last_used[i] < oldest) {
            oldest = last_used[i];
            victim = i;
        }
    }
    return victim;
}

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

int main(int argc, char *argv[]) {
    FILE *address_file;
    FILE *backing_store;
    FILE *out1;
    FILE *out2;
    FILE *out3;
    int logical_address;
    int frames = DEFAULT_FRAME_COUNT;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s addresses.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

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

    out1 = fopen("out1.txt", "w");
    out2 = fopen("out2.txt", "w");
    out3 = fopen("out3.txt", "w");
    if (out1 == NULL || out2 == NULL || out3 == NULL) {
        perror("output file");
        fclose(address_file);
        fclose(backing_store);
        if (out1) fclose(out1);
        if (out2) fclose(out2);
        if (out3) fclose(out3);
        return EXIT_FAILURE;
    }

    init_system(frames);

    while (fscanf(address_file, "%d", &logical_address) == 1) {
        int masked_address = logical_address & 0xFFFF;
        int page = (masked_address >> 8) & 0xFF;
        int offset = masked_address & 0xFF;
        int frame = search_tlb(page);
        int physical_address;
        int value;

        total_addresses++;
        access_clock++;

        if (frame != INVALID) {
            tlb_hits++;
        } else {
            frame = page_table[page];
            if (frame == INVALID) {
                frame = load_page(backing_store, page);
            }
            add_to_tlb(page, frame);
        }

        last_used[frame] = access_clock;
        physical_address = (frame * PAGE_SIZE) + offset;
        value = physical_memory[physical_address];

        fprintf(out1, "%d\n", masked_address);
        fprintf(out2, "%d\n", physical_address);
        fprintf(out3, "%d\n", value);
    }

    printf("Frame Count = %d\n", frames);
    printf("Number of Translated Addresses = %d\n", total_addresses);
    printf("Page Faults = %d\n", page_faults);
    printf("Page Fault Rate = %.3f\n", total_addresses ? (double) page_faults / total_addresses : 0.0);
    printf("TLB Hits = %d\n", tlb_hits);
    printf("TLB Hit Rate = %.3f\n", total_addresses ? (double) tlb_hits / total_addresses : 0.0);

    fclose(address_file);
    fclose(backing_store);
    fclose(out1);
    fclose(out2);
    fclose(out3);
    return EXIT_SUCCESS;
}