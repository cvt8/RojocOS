#include "kernel.h"
#include "lib.h"

#define HEAP_MAXPAGES 1024
#define MIN_ALLOC_SIZE 16
#define ALIGNMENT 16

// Block header for allocated and free blocks
typedef struct block_header {
    size_t size;        // Size of the block (excluding header)
    struct block_header* next; // Pointer to next free block (for free blocks only)
} block_header;

// Static heap management variables
static uintptr_t heap_pageinfo[HEAP_MAXPAGES];
static unsigned int heap_pagecount = 0;
static block_header* free_list = NULL;

// Align a size to the nearest multiple of ALIGNMENT
static inline size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

// Extend the heap by allocating a new page
void extend_heap() {
    if (heap_pagecount >= HEAP_MAXPAGES) {
        log_printf("extend_heap: No more heap pages available\n");
        return;
    }

    uintptr_t ptr = page_alloc(PO_KERNEL_HEAP);
    if (ptr == (uintptr_t)NULL) {
        log_printf("extend_heap: Failed to allocate page\n");
        return;
    }

    // Zero the page for security
    memset((void*)ptr, 0, PAGESIZE);

    // Map the page into the kernel's page table
    virtual_memory_map(kernel_pagetable, ptr, ptr, PAGESIZE, PTE_P | PTE_W, NULL);

    // Add to heap_pageinfo
    heap_pageinfo[heap_pagecount] = ptr;
    heap_pagecount++;

    // Initialize the new page as a single free block
    block_header* block = (block_header*)ptr;
    block->size = PAGESIZE - sizeof(block_header);
    block->next = free_list;
    free_list = block;

    log_printf("extend_heap: Allocated page at %p, size %zu\n", ptr, block->size);
}

// Allocate memory of the requested size
void* kernel_malloc(size_t size) {
    if (size == 0 || size > PAGESIZE - sizeof(block_header)) {
        log_printf("kernel_malloc: Invalid size %zu\n", size);
        return NULL;
    }

    // Align the size and include header
    size_t alloc_size = align_size(size + sizeof(block_header));

    // Search for a suitable free block (first-fit)
    block_header** prev = &free_list;
    block_header* current = free_list;

    while (current != NULL) {
        if (current->size >= alloc_size) {
            // Found a suitable block
            if (current->size >= alloc_size + sizeof(block_header) + MIN_ALLOC_SIZE) {
                // Split the block if enough space remains
                block_header* new_block = (block_header*)((char*)current + alloc_size);
                new_block->size = current->size - alloc_size;
                new_block->next = current->next;
                current->size = alloc_size - sizeof(block_header);
                *prev = new_block;
            } else {
                // Use the entire block
                *prev = current->next;
            }

            // Mark as allocated by setting next to NULL
            current->next = NULL;

            // Zero the data portion for security
            memset((char*)current + sizeof(block_header), 0, current->size);

            log_printf("kernel_malloc: Allocated %zu bytes at %p\n", size, (char*)current + sizeof(block_header));
            return (char*)current + sizeof(block_header);
        }
        prev = &current->next;
        current = current->next;
    }

    // No suitable block found; extend the heap
    extend_heap();
    if (free_list == NULL) {
        log_printf("kernel_malloc: Heap extension failed\n");
        return NULL;
    }

    // Retry allocation
    return kernel_malloc(size);
}

// Free allocated memory
void kernel_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    // Get the block header
    block_header* block = (block_header*)((char*)ptr - sizeof(block_header));

    // Validate the pointer
    int valid = 0;
    for (unsigned i = 0; i < heap_pagecount; i++) {
        uintptr_t page_start = heap_pageinfo[i];
        if ((uintptr_t)block >= page_start && (uintptr_t)block < page_start + PAGESIZE) {
            valid = 1;
            break;
        }
    }

    if (!valid || block->next != NULL) {
        log_printf("kernel_free: Invalid pointer %p\n", ptr);
        return;
    }

    // Zero the block for security
    memset(ptr, 0, block->size);

    // Add back to free list
    block->next = free_list;
    free_list = block;

    // Coalesce with adjacent free blocks
    block_header* current = free_list;
    block_header** prev = &free_list;
    while (current != NULL) {
        block_header* next = current->next;
        if (next != NULL && (char*)current + sizeof(block_header) + current->size == (char*)next) {
            // Merge with next block
            current->size += sizeof(block_header) + next->size;
            current->next = next->next;
        } else {
            prev = &current->next;
            current = current->next;
        }
    }

    log_printf("kernel_free: Freed %zu bytes at %p\n", block->size, ptr);
}

// Test the malloc implementation
void testmalloc(char* arg) {
    log_printf("testmalloc(%s)\n", arg ? arg : "NULL");

    static int test_count = 0;
    test_count++;
    log_printf("testmalloc: Test run %d\n", test_count);

    // Test 1: Simple allocation and free
    void* p1 = kernel_malloc(100);
    if (p1) {
        log_printf("testmalloc: Allocated 100 bytes at %p\n", p1);
        kernel_free(p1);
        log_printf("testmalloc: Freed 100 bytes at %p\n", p1);
    } else {
        log_printf("testmalloc: Allocation of 100 bytes failed\n");
    }

    // Test 2: Multiple allocations
    void* p2 = kernel_malloc(200);
    void* p3 = kernel_malloc(300);
    if (p2 && p3) {
        log_printf("testmalloc: Allocated 200 bytes at %p, 300 bytes at %p\n", p2, p3);
        kernel_free(p2);
        kernel_free(p3);
        log_printf("testmalloc: Freed 200 bytes at %p, 300 bytes at %p\n", p2, p3);
    } else {
        log_printf("testmalloc: Multiple allocations failed\n");
    }

    // Test 3: Large allocation
    void* p4 = kernel_malloc(2048);
    if (p4) {
        log_printf("testmalloc: Allocated 2048 bytes at %p\n", p4);
        kernel_free(p4);
        log_printf("testmalloc: Freed 2048 bytes at %p\n", p4);
    } else {
        log_printf("testmalloc: Large allocation failed\n");
    }

    // Test 4: Stress test with many small allocations
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kernel_malloc(50);
        if (ptrs[i]) {
            log_printf("testmalloc: Allocated 50 bytes at %p\n", ptrs[i]);
        }
    }
    for (int i = 0; i < 10; i++) {
        if (ptrs[i]) {
            kernel_free(ptrs[i]);
            log_printf("testmalloc: Freed 50 bytes at %p\n", ptrs[i]);
        }
    }

    // Test 5: Edge case - zero size
    void* p5 = kernel_malloc(0);
    if (p5 == NULL) {
        log_printf("testmalloc: Correctly rejected zero-size allocation\n");
    }

    // If arg is provided, attempt to parse as a size for allocation
    if (arg) {
        size_t size = atoi(arg);
        if (size > 0 && size <= PAGESIZE - sizeof(block_header)) {
            void* p6 = kernel_malloc(size);
            if (p6) {
                log_printf("testmalloc: Allocated %zu bytes at %p based on arg\n", size, p6);
                kernel_free(p6);
                log_printf("testmalloc: Freed %zu bytes at %p\n", size, p6);
            } else {
                log_printf("testmalloc: Allocation of %zu bytes failed\n", size);
            }
        } else {
            log_printf("testmalloc: Invalid size %zu from arg\n", size);
        }
    }

    log_printf("testmalloc: Test complete\n");
}
