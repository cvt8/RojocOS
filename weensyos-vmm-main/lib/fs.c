#include "fs.h"

#define METADATA_SIZE sizeof(fs_metadata)
#define BLOCK_SIZE 4096

// Pour nombres positifs uniquement
#define CEIL_DIV(x,n) (((x) + (n) - 1) / (n))



struct fs_inode_entry {
    uint64_t start_block;
    uint64_t block_count;
    unsigned char cipher_key[256];
};

#define INODE_ENTRY_SIZE sizeof(struct fs_inode_entry)


int search_free_blocks(fs_descriptor *fsdesc, int n) {
    int start_block = 0;
    int count = 0;
    uintptr_t addr = fsdesc->avail_block_table_offset;

    while (start_block+count < fsdesc->metadata.block_count && count < n) {
        unsigned char dest;

        if (fsdesc->fsdr(&dest, addr, 1) < 0) {
            return -1;
        }

        count++;

        if (dest != 0) {
            start_block = start_block + count;
            count = 0;
        }

        addr += 1;
    }

    if (count == n) {
        return start_block;
    }
    
    return -1;
}

int are_blocks_avaiblable(fs_descriptor *fsdesc, int start_block, int n) {
    uintptr_t addr = fsdesc->avail_block_table_offset;

    for (int i = 0; i < n; i++) {
        unsigned char dest;

        if (fsdesc->fsdr(&dest, addr + start_block + i, 1) < 0) {
            return -1;
        }

        if (dest != 0) {
            return 0;
        }

        addr += 1;
    }

    return 1;
}



int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr, fs_disk_writer fsdw) {
    fsdesc->fsdr = fsdr;
    fsdesc->fsdw = fsdw;

    int r = fsdr(&fsdesc->metadata, 0, METADATA_SIZE);
    if (r < 0) {
        return r;
    }

    fsdesc->data_offset = METADATA_SIZE + fsdesc->metadata.inode_count * INODE_ENTRY_SIZE + ...;

    return 0;
}

int fs_read(fs_descriptor *fsdesc, fs_ino ino, char *buf, size_t size, off_t offset) {
    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;

    struct fs_inode_entry entry;

    int r = fsdesc->fsdr(&entry, entry_addr, INODE_ENTRY_SIZE);
    if (r < 0) {
        return r;
    }

    uintptr_t start_addr = fsdesc->data_offset + entry.start_block * BLOCK_SIZE + offset;

    int r = fsdesc->fsdr(buf, start_addr, size);
    if (r < 0) {
        return r;
    }
}

int fs_write(fs_descriptor *fsdesc, fs_ino ino, char *buf, size_t size, off_t offset) {
    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;

    struct fs_inode_entry entry;

    int r = fsdesc->fsdr(&entry, entry_addr, INODE_ENTRY_SIZE);
    if (r < 0) {
        return r;
    }


    int offset_block = CEIL_DIV(offset, BLOCK_SIZE);

    if (offset_block > entry.block_count) {
        int new_block = offset_block - entry.block_count;
        

        
        // Check if new_block are avaible at 
    }

    uintptr_t start_addr = fsdesc->data_offset + entry.start_block * BLOCK_SIZE + offset;

    int r = fsdesc->fsdw(buf, start_addr, size);
    if (r < 0) {
        return r;
    }
}
