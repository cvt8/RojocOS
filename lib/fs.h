#ifndef WEENSYOS_FS_H
#define WEENSYOS_FS_H

#include "lib.h"

typedef unsigned int fs_ino;

typedef int (*fs_disk_reader)(uintptr_t ptr, uint64_t start, size_t size);
typedef int (*fs_disk_writer)(uintptr_t ptr, uint64_t start, size_t size);

typedef struct fs_metadata {
    uint32_t inode_count; /* data index */
    uint32_t block_count;
    uint32_t node_count; /* fs tree nodes */
} fs_metadata;

typedef struct fs_descriptor {
    fs_disk_reader fsdr;
    fs_disk_writer fsdw;
    fs_metadata metadata;
    uintptr_t avail_block_table_offset;
    uintptr_t inode_table_offset;
    uintptr_t block_usage_offset;
    uintptr_t tree_usage_offset;
    uintptr_t tree_offset;
    uintptr_t data_offset;
} fs_descriptor;

 

int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr, fs_disk_writer fsdw);

// return value is negative if an error occured
// return value is 0 is it is a directory
// return value is positive if it is a file, the value is the inode of the data
int fs_getattr(fs_descriptor *fsdesc, const char *path);

int fs_truncate(fs_descriptor *fsdesc, fs_ino ino, off_t size);

int fs_read(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, off_t offset);

int fs_write(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, off_t offset);

#endif
