

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>


#define FRAME_SIZE 256          //FRAME SIZE is 256 bytes 2^8
#define FRAMES 256              //Number of frames
#define PAGE_SIZE 256           // PAGE SIZE is 256 bytes 2^8
#define TLB_SIZE 16             //Number of TLB Entries
#define PAGE_MASKER 0xFFFF      // This masker will be explained later.
#define OFFSET_MASKER 0xFF      // This masker will be explained later.
#define ADDRESS_SIZE 10

// input files
FILE *address_file;
FILE *backing_store;

// For ease of access
struct page_frame
{
    int page_number;
    int frame_number;
};

// initial global variables
int Physical_Memory [FRAMES][FRAME_SIZE];
struct page_frame TLB[TLB_SIZE];
struct page_frame PAGE_TABLE[FRAMES];
char address [ADDRESS_SIZE]; // to store the address coming from the input file
int TLBHits = 0;
int page_faults = 0;
char buffer[PAGE_SIZE];
int firstAvailableFrame = 0;
int firstAvailablePageTableIndex = 0;
signed char value;
int TLB_Full_Entries = 0;

// Templates for predefined use
int read_from_store(int pageNumber);
void insert_into_TLB(struct page_frame);

// get a page for virtual address
void get_page(int logical_address) {
    struct page_frame pageTableEntry;
    pageTableEntry.page_number = ((logical_address & PAGE_MASKER)>>8); // Masks value of logical address and shifts 8 bits right to get page number
    pageTableEntry.frame_number = -1; //Negative for failure tests

    int offset = (logical_address & OFFSET_MASKER); // Fetch page offset


    // look through TLB for a match
    for(int i = 0; i < TLB_SIZE; i++) {
        if(TLB[i].page_number == pageTableEntry.page_number) {
            pageTableEntry.frame_number = TLB[i].frame_number;
            TLBHits++;
        }
    }

    // if the frameNumber was not int the TLB search pagetable
    if(pageTableEntry.frame_number == -1) {
        for(int i = 0; i < firstAvailablePageTableIndex; i++) {
            if(PAGE_TABLE[i].page_number == pageTableEntry.page_number) {
                pageTableEntry.frame_number = PAGE_TABLE[i].frame_number;
            }
        }

        //Handle page faults
        if(pageTableEntry.frame_number == -1) {
            pageTableEntry.frame_number = read_from_store(pageTableEntry.page_number);
            page_faults++;
        }
    }

    //Add page and frame
    insert_into_TLB(pageTableEntry);
    value = Physical_Memory[pageTableEntry.frame_number][offset];


    // output final values to a file named results
    FILE *results = fopen("results.txt","a");
    fprintf(results,"Virtual address: %d Physical address: %d Value: %d\n", logical_address, (pageTableEntry.frame_number << 8) | offset, value);
    fclose(results);
}

int read_from_store(int pageNumber) {

    //Search from beginning of file to pagesize byte
    if (fseek(backing_store, pageNumber * PAGE_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }

    // read 256 byte chunk to buffer
    if (fread(buffer, sizeof(signed char), PAGE_SIZE, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");
    }

    // load the bits into physical memory
    for(int i = 0; i < PAGE_SIZE; i++) {
        Physical_Memory[firstAvailableFrame][i] = buffer[i];
    }

    PAGE_TABLE[firstAvailablePageTableIndex].page_number = pageNumber;
    PAGE_TABLE[firstAvailablePageTableIndex].frame_number = firstAvailableFrame;

    firstAvailableFrame++;
    firstAvailablePageTableIndex++;
    return PAGE_TABLE[firstAvailablePageTableIndex-1].frame_number;
}

//Uses FIFO algorithm
void insert_into_TLB(struct page_frame pageTableEntry) {

    //Search TLB for table entry and shift to end if found, moving everything over.
    int i;
    for(i = 0; i < TLB_Full_Entries; i++) {
        if(TLB[i].page_number == pageTableEntry.page_number) {
            for(i = i; i < TLB_Full_Entries - 1; i++)
                TLB[i] = TLB[i + 1];
            break;
        }
    }

    //Insert into table if loop was completed (entry not found in table)
    if(i == TLB_Full_Entries) {
        for (int j=0; j < i; j++) {
            TLB[j] = TLB[j + 1];
        }
    }
    TLB[i].page_number = pageTableEntry.page_number;        // and insert the page and frame on the end
    TLB[i].frame_number = pageTableEntry.frame_number;

    if(TLB_Full_Entries < TLB_SIZE-1) {
        TLB_Full_Entries++;
    }
}

int main(int argc, char *argv[]) {
    printf("Results are printed to the \"results.txt\" file.\n");

    // Basic input error checking
    if (argc != 2) {
        fprintf(stderr,"Usage: ./a.out [input file]\n");
        return -1;
    }

    // Open input file from the command line arguments
    address_file = fopen(argv[1], "r");

    //open backing store
    backing_store = fopen("BACKING_STORE.bin", "rb");


    //File error detection
    if (address_file == NULL) {
        fprintf(stderr, "Error opening addresses.txt %s\n",argv[1]);
        return -1;
    }

    if (backing_store == NULL) {
        fprintf(stderr, "Error opening BACKING_STORE.bin %s\n","BACKING_STORE.bin");
        return -1;
    }


    int translated_addresses = 0;
    int logical_address;
    // read through the input file and output each logical address
    while (fgets(address, ADDRESS_SIZE, address_file) != NULL) {
        logical_address = atoi(address);

        get_page(logical_address);
        translated_addresses++;
    }

    // Summarize performance stats
    printf("Number of translated addresses = %d\n", translated_addresses);
    double pfRate = page_faults / (double)translated_addresses;
    double TLBRate = TLBHits / (double)translated_addresses;

    printf("Page Faults = %d\n", page_faults);
    printf("Page Fault Rate = %.3f\n",pfRate);
    printf("TLB Hits = %d\n", TLBHits);
    printf("TLB Hit Rate = %.3f\n", TLBRate);

    // close open files
    fclose(address_file);
    fclose(backing_store);

    return 0;
}