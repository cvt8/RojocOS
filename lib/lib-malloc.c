#include "lib.h"
#include "process.h"

static uint8_t* heap_top = (uint8_t*) 0x200000;
static uint8_t* page_top = (uint8_t*) 0x200000;

void* malloc(size_t size) {
    while (ROUNDDOWN(heap_top + size, PAGESIZE) >= page_top) {
        int r = sys_page_alloc(page_top);
        assert(r >= 0);
        page_top += PAGESIZE;
    }

    void* addr = heap_top;
    heap_top += size;
    return addr;
}
