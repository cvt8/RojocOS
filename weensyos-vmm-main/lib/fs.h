#ifndef WEENSYOS_FS_H
#define WEENSYOS_FS_H

#include "lib.h"

typedef unsigned int fs_ino;

typedef int (*fs_disk_reader)(uintptr_t ptr, uint64_t start, size_t size);
typedef int (*fs_disk_writer)(uintptr_t ptr, uint64_t start, size_t size);

typedef struct fs_metadata {
    uint64_t inode_count; /* data index */
    uint64_t block_count;
    uint64_t node_count; /* fs tree nodes */
} fs_metadata;

typedef struct fs_descriptor {
    fs_disk_reader fsdr;
    fs_disk_writer fsdw;
    fs_metadata metadata;
    uintptr_t avail_block_table_offset;
    uintptr_t data_offset;
} fs_descriptor;

 

int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr, fs_disk_writer fsdw);

int fs_getattr(fs_descriptor *fsdesc, const char *path, struct fs_stat *stbuf);

int fs_truncate(fs_descriptor *fsdesc, fs_ino ino, off_t size,
        struct fs_file_info *fi);

int fs_read(fs_descriptor *fsdesc, fs_ino ino, char *buf, size_t size,
        off_t offset, struct fs_file_info *fi);

int fs_write(fs_descriptor *fsdesc, fs_ino ino, const char *buf, size_t size,
        off_t offset, struct fs_file_info *fi);

#endif
