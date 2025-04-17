#include "kernel.h"
#include "lib.h"

#define HEAP_MAXPAGES 1024
//PAGENUMBER(MEMSIZE_PHYSICAL)

static uintptr_t heap_pageinfo[HEAP_MAXPAGES];
static unsigned int heap_pagecount = 0;

void extend_heap() {
    log_printf("arfze \n");
    uintptr_t ptr = page_alloc(PO_KERNEL_HEAP);
    log_printf("kbefkj \n");
    heap_pageinfo[heap_pagecount] = ptr;
    heap_pagecount += 1;
}

void *kernel_malloc(size_t size) {
    return NULL;
}

void kernel_free(void *ptr) {

}

void testmalloc(char *arg) {
    

    log_printf("testmalloc(%s)\n", arg);

    static int s = 0;
    log_printf("testmalloc : s = %d\n", s);
    s += 1;

    extend_heap();
}


