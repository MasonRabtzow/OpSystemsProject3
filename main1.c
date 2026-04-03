#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16

int main() {
    FILE *address_file = fopen("addresses.txt", "r");
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

    char buffer[10];
    int virtual_address;
    
    // Page Table: -1 indicates the page is not in memory
    int page_table[PAGE_TABLE_SIZE];
    for(int i = 0; i < PAGE_TABLE_SIZE; i++) page_table[i] = -1;

    while (fgets(buffer, sizeof(buffer), address_file) != NULL) {
        virtual_address = atoi(buffer);

        // 1. Extract bits 0-15 logic
        unsigned char page_num = (virtual_address >> 8) & 0xFF;
        unsigned char offset = virtual_address & 0xFF;

        int frame_num = -1;

        // 2. TLB Lookup (Logic to be implemented by you)
        
        // 3. Page Table Lookup
        if (page_table[page_num] != -1) {
            frame_num = page_table[page_num];
        } else {
            // 4. Page Fault Handling
            // Read from backing_store into physical memory...
            // frame_num = next_available_frame++;
            // page_table[page_num] = frame_num;
        }

        int physical_address = (frame_num << 8) | offset;
        printf("Virtual: %d Physical: %d\n", virtual_address & 0xFFFF, physical_address);
    }

    return 0;
}