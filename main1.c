#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define DEFAULT_FRAME_COUNT 128
#define MAX_FRAME_COUNT 1024
#define INVALID -1


struct TLBEntry_ {
    int logical_page;
    int frame_number;
    int valid;
} TLBEntry;


int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <addresses.txt>\n", argv[0]);
        return 1;
    }

    // Open files
    FILE *address_file = fopen("addresses.txt", "r");
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    if (address_file == NULL || backing_store == NULL) {
        printf("Error: Could not open necessary files.\n");
        return 1;
    }

    // System Data Structures
    struct TLBEntry tlb[TLB_SIZE];
    int page_table[PAGE_TABLE_SIZE];
    int8_t physical_memory[NUM_FRAMES][FRAME_SIZE]; // int8_t handles signed byte values
    
    // Page Table: -1 indicates the page is not in memory
    // Initialization
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
    }
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i] = -1; // -1 means not in memory
    }

    // State Variables
    int next_free_frame = 0;
    int tlb_pointer = 0; // For FIFO replacement
    
    // Statistics
    int total_addresses = 0;
    int tlb_hits = 0;
    int page_faults = 0;

    char buffer[10];
    int logical_address;




   // Main translation loop
    while (fgets(buffer, sizeof(buffer), address_file) != NULL) {
        logical_address = atoi(buffer);
        total_addresses++;

        // Extract page number and offset
        int page_num = (logical_address >> 8) & 0xFF;
        int offset = logical_address & 0xFF;

        int frame_num = -1;
        int tlb_hit_found = 0;

        // 1. Search TLB
        for (int i = 0; i < TLB_SIZE; i++) {
            if (tlb[i].valid && tlb[i].logical_page == page_num) {
                frame_num = tlb[i].frame_number;
                tlb_hit_found = 1;
                tlb_hits++;
                break;
            }
        }

        // 2. Search Page Table (if TLB Miss)
        if (!tlb_hit_found) {
            if (page_table[page_num] != -1) {
                // Page Table Hit
                frame_num = page_table[page_num];
            } else {
                // 3. Page Fault (Not in memory, read from backing store)
                page_faults++;
                
                // Seek to the correct page in the backing store
                fseek(backing_store, page_num * PAGE_SIZE, SEEK_SET);
                
                // Read the 256-byte page into the next available physical frame
                fread(physical_memory[next_free_frame], sizeof(int8_t), PAGE_SIZE, backing_store);
                
                // Update page table and track the frame
                frame_num = next_free_frame;
                page_table[page_num] = frame_num;
                next_free_frame++;
            }

        // 4. Update TLB using FIFO
            tlb[tlb_pointer].logical_page = page_num;
            tlb[tlb_pointer].frame_number = frame_num;
            tlb[tlb_pointer].valid = 1;
            
            // Move pointer and wrap around if necessary
            tlb_pointer = (tlb_pointer + 1) % TLB_SIZE;
        }

        // 5. Calculate Physical Address and Fetch Value
        int physical_address = (frame_num << 8) | offset;
        int8_t value = physical_memory[frame_num][offset];

        printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
    }

    // Print final statistics
    printf("\n--- Statistics ---\n");
    printf("Page-fault rate: %.2f%%\n", ((float)page_faults / total_addresses) * 100);
    printf("TLB hitrate: %.2f%%\n", ((float)tlb_hits / total_addresses) * 100);

    // Clean up
    fclose(address_file);
    fclose(backing_store);

    return 0;
}