#ifndef WEENSYOS_K_MALLOC_H
#define WEENSYOS_K_MALLOC_H

#include "lib.h"

void *kernel_malloc(size_t size);
void kernel_free(void *ptr);
void testmalloc(char *arg);

#endif
