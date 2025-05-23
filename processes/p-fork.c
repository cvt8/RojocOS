#include "process.h"
#include "lib.h"
#define ALLOC_SLOWDOWN 100

extern uint8_t end[];

// These global variables go on the data page.
uint8_t* heap_top;
uint8_t* stack_bottom;

void process_main(void) {
    // Fork a total of three new copies.
    pid_t p1 = sys_fork();
    assert(p1 >= 0);
    pid_t p2 = sys_fork();
    assert(p2 >= 0);

    // Check fork return values: fork should return 0 to child.
    if (sys_getpid() == 1) {
        //assert(p1 != 0);
        assert(p1 != 0 && p2 != 0 && p1 != p2);
    } else {
        //assert(p1 == 0);
        assert(p1 == 0 || p2 == 0);
    }

    while (1) {
        sys_yield();
    }

    // The rest of this code is like p-allocator.c.

    pid_t p = sys_getpid();
    srand(p);

    // The heap starts on the page right after the 'end' symbol,
    // whose address is the first address not allocated to process code
    // or data.
    heap_top = ROUNDUP((uint8_t*) end, PAGESIZE);

    // The bottom of the stack is the first address on the current
    // stack page (this process never needs more than one stack page).
    stack_bottom = ROUNDDOWN((uint8_t*) read_rsp() - 1, PAGESIZE);

    // Allocate heap pages until (1) hit the stack (out of address space)
    // or (2) allocation fails (out of physical memory).
    while (1) {
        if ((rand() % ALLOC_SLOWDOWN) < p) {
            if (heap_top == stack_bottom || sys_page_alloc(heap_top) < 0) {
                break;
            }
            *heap_top = p;      /* check we have write access to new page */
            console[CPOS(24, 79)] = '0' + p;  /* check we can write to console */
            heap_top += PAGESIZE;
        }
        sys_yield();
    }

    // After running out of memory, do nothing forever
    while (1) {
        sys_yield();
    }
}
