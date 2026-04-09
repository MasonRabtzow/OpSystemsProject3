#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 256
#define PAGE_SIZE 256
#define FRAME_SIZE 256
#define NUM_FRAMES 256

// TLB Entry Structure
struct TLBEntry {
    int logical_page;
    int frame_number;
    int valid;
};

// --- Function Prototypes ---
void init_system(struct TLBEntry tlb[], int page_table[]);
int search_tlb(struct TLBEntry tlb[], int page_num);
void update_tlb(struct TLBEntry tlb[], int page_num, int frame_num, int *tlb_pointer);
int resolve_page(int page_num, int page_table[], int8_t physical_memory[][FRAME_SIZE], 
                 int *next_free_frame, FILE *backing_store, int *page_faults);

// --- Main Execution ---
int main(int argc, char *argv[]) {
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

    // System Data Structures
    struct TLBEntry tlb[TLB_SIZE];
    int page_table[PAGE_TABLE_SIZE];
    int8_t physical_memory[NUM_FRAMES][FRAME_SIZE];

    // State Variables & Stats
    int next_free_frame = 0;
    int tlb_pointer = 0; 
    int total_addresses = 0;
    int tlb_hits = 0;
    int page_faults = 0;

    // Initialize TLB and Page Table
    init_system(tlb, page_table);

    char buffer[10];
    int logical_address;

    // Main translation loop
    while (fgets(buffer, sizeof(buffer), address_file) != NULL) {
        logical_address = atoi(buffer);
        total_addresses++;

        // Extract page number and offset
        int page_num = (logical_address >> 8) & 0xFF;
        int offset = logical_address & 0xFF;

        // 1. Try to find the frame in the TLB
        int frame_num = search_tlb(tlb, page_num);

        if (frame_num != -1) {
            tlb_hits++; // TLB Hit
        } else {
            // 2. TLB Miss: Check Page Table and handle Page Faults if necessary
            frame_num = resolve_page(page_num, page_table, physical_memory, 
                                     &next_free_frame, backing_store, &page_faults);
            
            // 3. Update the TLB with the newly found/loaded frame
            update_tlb(tlb, page_num, frame_num, &tlb_pointer);
        }

        // 4. Calculate Physical Address and Fetch Value
        int physical_address = (frame_num << 8) | offset;
        int8_t value = physical_memory[frame_num][offset];

        //5. Write to output files
        fprintf(out1, "%d\n", logical_address);
        fprintf(out2, "%d\n", physical_address);
        fprintf(out3, "%d\n", value);

        
    }

    // Print final statistics
    printf("\n--- Statistics ---\n");
    printf("Page-fault rate: %.2f%%\n", ((float)page_faults / total_addresses) * 100);
    printf("TLB hit rate: %.2f%%\n", ((float)tlb_hits / total_addresses) * 100);


    fclose(address_file);
    fclose(backing_store);
    fclose(out1);
    fclose(out2);
    fclose(out3);
    return 0;
}

// --- Function Implementations ---

void init_system(struct TLBEntry tlb[], int page_table[]) {
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
    }
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i] = -1;
    }
}

int search_tlb(struct TLBEntry tlb[], int page_num) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].logical_page == page_num) {
            return tlb[i].frame_number; // Hit
        }
    }
    return -1; // Miss
}

void update_tlb(struct TLBEntry tlb[], int page_num, int frame_num, int *tlb_pointer) {
    // Insert new entry at the current FIFO pointer
    tlb[*tlb_pointer].logical_page = page_num;
    tlb[*tlb_pointer].frame_number = frame_num;
    tlb[*tlb_pointer].valid = 1;
    
    // Move the pointer forward, wrapping around at TLB_SIZE
    *tlb_pointer = (*tlb_pointer + 1) % TLB_SIZE;
}

int resolve_page(int page_num, int page_table[], int8_t physical_memory[][FRAME_SIZE], 
                 int *next_free_frame, FILE *backing_store, int *page_faults) {
    
    // Check if page is already in memory (Page Table Hit)
    if (page_table[page_num] != -1) {
        return page_table[page_num];
    }
    
    // Page Fault (Not in memory)
    (*page_faults)++;
    int frame_to_use = *next_free_frame;

    // Read the 256-byte page from disk into physical memory
    fseek(backing_store, page_num * PAGE_SIZE, SEEK_SET);
    fread(physical_memory[frame_to_use], sizeof(int8_t), PAGE_SIZE, backing_store);
    
    // Update the page table to point to the new frame
    page_table[page_num] = frame_to_use;
    
    // Increment the free frame tracker
    (*next_free_frame)++;

    return frame_to_use;
}
