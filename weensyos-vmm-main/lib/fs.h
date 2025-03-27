#ifndef WEENSYOS_FS_H
#define WEENSYOS_FS_H

#include "lib.h"

typedef unsigned int fs_ino;

typedef int (*fs_disk_reader)(uintptr_t ptr, uint64_t start, size_t size);

typedef struct fs_descriptor {
    fs_ino root_ino;
    fs_disk_reader fsdr;
} fs_descriptor;


int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr);

int fs_read(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, int offset);

/*int fs_getattr(fs_descriptor *fsdesc, const char *path, struct fs_stat *stbuf, struct fs_file_info *fi);

int fs_truncate(fs_descriptor *fsdesc, const char *path, off_t size,
        struct fs_file_info *fi);

int fs_open(fs_descriptor *fsdesc, const char *path, struct fs_file_info *fi);

int fs_read(fs_descriptor *fsdesc, const char *path, char *buf, size_t size,
        off_t offset, struct fs_file_info *fi);

int fs_write(fs_descriptor *fsdesc, const char *path, const char *buf, size_t size,
        off_t offset, struct fs_file_info *fi);*/

#endif
