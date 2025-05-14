#ifndef WEENSYOS_K_HARDWARE_H
#define WEENSYOF_K_HARDWARE_H

#include "lib.h"

void readseg(uintptr_t ptr, uint32_t src_sect,
        size_t filesz, size_t memsz);


int readdisk(uintptr_t ptr, uint64_t start, uint64_t size);

int writedisk(uintptr_t ptr, uint64_t start, size_t size);

#endif
