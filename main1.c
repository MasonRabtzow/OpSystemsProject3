#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 256
#define PAGE_SIZE 256
#define FRAME_SIZE 256
#define NUM_FRAMES 256
#define INVALID -1

// TLB Entry Structure 
typedef struct TLBEntry {
    int page;
    int frame; 
    int valid;
} TLBEntry;

// --- Function Prototypes ---
void init_system(TLBEntry tlb[], int page_table[]);
int search_tlb(TLBEntry tlb[], int page);
void update_tlb(TLBEntry tlb[], int page, int frame, int *tlb_fifo_index);
int load_page(int page, int page_table[], int8_t physical_memory[][FRAME_SIZE], 
                 int *next_free_frame, FILE *backing_store, int *page_faults);

// --- Main Execution ---
int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s addresses.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *address_file = fopen(argv[1], "r");
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    // Open output files
    FILE *out1 = fopen("out1.txt", "w");
    FILE *out2 = fopen("out2.txt", "w");
    FILE *out3 = fopen("out3.txt", "w");

    if (address_file == NULL || backing_store == NULL || out1 == NULL || out2 == NULL || out3 == NULL) {
        printf("Error: Could not open one or more necessary files.\n");
        return 1;
    }

    // Physical memory & mappings
    TLBEntry tlb[TLB_SIZE];
    int page_table[PAGE_TABLE_SIZE];
    int8_t physical_memory[NUM_FRAMES][FRAME_SIZE];

    // State Variables & Stats
    int next_free_frame = 0;
    int tlb_fifo_index = 0; 
    int total_addresses = 0;
    int tlb_hits = 0;
    int page_faults = 0;

    // Initialize TLB and Page Table
    init_system(tlb, page_table);

    int logical_address;

    // Main translation loop
    while (fscanf(address_file, "%d", &logical_address) == 1) {
        total_addresses++;

        // Extract page and offset
        int page = (logical_address >> 8) & 0xFF; 
        int offset = logical_address & 0xFF;

        // Search TLB
        int frame = search_tlb(tlb, page); 

        if (frame != INVALID) {
            tlb_hits++; // TLB Hit
        } else {
            // 2. Search page table
            frame = page_table[page];

            if (frame == INVALID) {
                frame = load_page(page, page_table, physical_memory, 
                                     &next_free_frame, backing_store, &page_faults);
            }
            
            // update TLB
            update_tlb(tlb, page, frame, &tlb_fifo_index);
        }

        // compute final physical address
        int physical_address = (frame << 8) | offset;
        int8_t value = physical_memory[frame][offset];

        // Write outputs
        fprintf(out1, "%d\n", logical_address);
        fprintf(out2, "%d\n", physical_address);
        fprintf(out3, "%d\n", value);
    }

    // Print stats
    printf("\n--- Statistics ---\n");
    printf("Number of Translated Addresses = %d\n", total_addresses);
    printf("Page Faults = %d\n", page_faults);
    printf("Page-fault rate: %.3f\n", (float)page_faults / total_addresses);
    printf("TLB Hits = %d\n", tlb_hits);
    printf("TLB hit rate: %.3f\n", (float)tlb_hits / total_addresses);

    fclose(address_file);
    fclose(backing_store);
    fclose(out1);
    fclose(out2);
    fclose(out3);
    return 0;
}

// --- Function Implementations ---

void init_system(TLBEntry tlb[], int page_table[]) {
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
        tlb[i].page = INVALID;
        tlb[i].frame = INVALID;
    }
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i] = INVALID;
    }
}

int search_tlb(TLBEntry tlb[], int page) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page == page) {
            return tlb[i].frame;
        }
    }
    return INVALID;
}

void update_tlb(TLBEntry tlb[], int page, int frame, int *tlb_fifo_index) {
    tlb[*tlb_fifo_index].page = page;
    tlb[*tlb_fifo_index].frame = frame;
    tlb[*tlb_fifo_index].valid = 1;
    *tlb_fifo_index = (*tlb_fifo_index + 1) % TLB_SIZE;
}

int load_page(int page, int page_table[], int8_t physical_memory[][FRAME_SIZE], 
                 int *next_free_frame, FILE *backing_store, int *page_faults) {
    
    // Page Fault logic
    (*page_faults)++;
    int frame_to_use = *next_free_frame;

    fseek(backing_store, page * PAGE_SIZE, SEEK_SET);
    fread(physical_memory[frame_to_use], sizeof(int8_t), PAGE_SIZE, backing_store);
    
    page_table[page] = frame_to_use;
    (*next_free_frame)++;

    return frame_to_use;
}
