#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "lib.h"
#include "string.h"

#define FS_IO_MAX_SIZE INT64_MAX
#define FS_KEY_SIZE 256
#define FS_IV_SIZE 16

typedef unsigned int fs_ino;

typedef int (*fs_disk_reader)(uintptr_t ptr, uint64_t start, size_t size);
typedef int (*fs_disk_writer)(uintptr_t ptr, uint64_t start, size_t size);
typedef void (*fs_random_generator)(uint8_t *buffer, size_t size);

typedef struct fs_metadata {
    uint32_t inode_count; /* data index */
    uint32_t block_count;
    uint32_t node_count; /* fs tree nodes */
} fs_metadata;

typedef struct fs_descriptor {
    fs_disk_reader fsdr;
    fs_disk_writer fsdw;
    fs_random_generator fsrng;

    fs_metadata metadata;
    
    uintptr_t avail_block_table_offset;
    uintptr_t inode_table_offset;
    uintptr_t block_usage_offset;
    uintptr_t tree_usage_offset;
    uintptr_t tree_offset;
    uintptr_t data_offset;
} fs_descriptor;


typedef struct {
    fs_descriptor *fsdesc;
    uint32_t node_index;
    int offset;
} fs_dirreader;
 

int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr, fs_disk_writer fsdw, fs_random_generator fsrng);

// return value is negative if an error occured
// return value is 0 is it is a directory
// return value is positive if it is a file, the value is the inode of the data
int64_t fs_getattr(fs_descriptor *fsdesc, normpath path);

int fs_truncate(fs_descriptor *fsdesc, fs_ino ino, off_t size);

ssize_t fs_read(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, uint64_t offset);

ssize_t fs_write(fs_descriptor *fsdesc, fs_ino ino, const void *buf, size_t size, uint64_t offset);

int fs_readdir_init(fs_descriptor *fsdesc, normpath path, fs_dirreader *dr);
int fs_readdir_next(fs_dirreader *dr, char *buffer);

int fs_touch(fs_descriptor *fsdesc, normpath parent, uint32_t value);

int fs_test(fs_descriptor *fsdesc);

int64_t fs_alloc_inode(fs_descriptor *fsdesc);

int fs_remove(fs_descriptor *fsdesc, normpath path);

#endif
