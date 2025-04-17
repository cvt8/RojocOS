#include "kernel.h"
#include "lib.h"

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
}
