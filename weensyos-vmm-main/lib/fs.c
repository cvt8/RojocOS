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


int set_availability(fs_descriptor *fsdesc, int block, unsigned char value) {
    uintptr_t addr = fsdesc->avail_block_table_offset + block;

    if (fsdesc->fsdw(&value, addr, 1) < 0) {
        return -1;
    }
    
    return 0;
}

int copy_block(fs_descriptor *fsdesc, int src, int dest) {
    char buf[BLOCK_SIZE];

    uintptr_t src_addr = fsdesc->data_offset + src * BLOCK_SIZE;
    uintptr_t dest_addr = fsdesc->data_offset + dest * BLOCK_SIZE;

    if (fsdesc->fsdr(buf, src_addr, BLOCK_SIZE) < 0) {
        return -1;
    }

    if (fsdesc->fsdw(buf, dest_addr, BLOCK_SIZE) < 0) {
        return -1;
    }

    return 0;
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

    int total_block = CEIL_DIV(offset+size, BLOCK_SIZE);
    if (total_block > entry.block_count) {
        int new_block = total_block - entry.block_count;
        
        if (are_blocks_avaiblable(fsdesc, entry.start_block + entry.block_count, new_block)) {
            for (int i = 0; i < new_block; i++) {
                set_availability(fsdesc, entry.start_block + entry.block_count + i, 1);
            }
        } else {
            int new_start_block = search_free_blocks(fsdesc, total_block);
            if (r < 0) {
                return -1;
            }

            for (int i = 0; i < entry.block_count; i++) {
                copy_block(fsdesc, entry.start_block + i, new_start_block + i);
                set_availability(fsdesc, entry.start_block + i, 0);
                set_availability(fsdesc, new_start_block + i, 1);
            }

            entry.start_block = new_start_block;
        }

        entry.block_count = total_block;
        fsdesc->fsdw(&entry, entry_addr, INODE_ENTRY_SIZE);
    }

    uintptr_t start_addr = fsdesc->data_offset + entry.start_block * BLOCK_SIZE + offset;

    int r = fsdesc->fsdw(buf, start_addr, size);
    if (r < 0) {
        return r;
    }
}
