#include "fs.h"

int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr) {
    fsdesc->root_ino = 0;
    fsdesc->fsdr = fsdr;
    return 0;
}

int fs_read(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, int offset) {
    int r = fsdesc->fsdr((uintptr_t) buf, offset, size);
    return r;
}
