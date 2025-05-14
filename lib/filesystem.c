#include "filesystem.h"

#define METADATA_SIZE sizeof(fs_metadata)
#define BLOCK_SIZE 4096

// Pour nombres positifs uniquement
#define SIZE_TO_BLOCK(x) ((uint32_t) (((x) + BLOCK_SIZE - 1) / BLOCK_SIZE))


struct fs_inode_entry {
    uint64_t start_block;
    uint64_t block_count;
    unsigned char cipher_key[256];
};

#define NAME_SIZE 32
#define MAX_CHILDREN 32

typedef struct fd_node_child {
    char name[NAME_SIZE];
    uint32_t index;
} fd_node_child_t;

typedef struct fs_node {
    uint8_t used;
    int value;
    int children_count;
    fd_node_child_t children[MAX_CHILDREN];
} fs_node_t;


#define NODE_SIZE sizeof(fs_node_t)


#define INODE_ENTRY_SIZE sizeof(struct fs_inode_entry)


static const uint8_t ZERO = 0;
static const uint8_t ONE = 1;



int64_t search_free_blocks(fs_descriptor *fsdesc, uint32_t n) {
    uint32_t start_block = 0;
    uint32_t count = 0;
    uintptr_t addr = fsdesc->avail_block_table_offset;

    while (start_block+count < fsdesc->metadata.block_count && count < n) {
        unsigned char dest;

        if (fsdesc->fsdr((uintptr_t) &dest, addr, 1) < 0) {
            return -1;
        }

        count++;

        if (dest != 0) {
            start_block += count;
            count = 0;
        }

        addr += 1;
    }

    if (count == n) {
        return (int64_t) start_block;
    }
    
    return -1;
}

int are_blocks_avaiblable(fs_descriptor *fsdesc, int start_block, int n) {
    uintptr_t addr = fsdesc->avail_block_table_offset;

    for (int i = 0; i < n; i++) {
        unsigned char dest;

        if (fsdesc->fsdr((uintptr_t) &dest, addr + start_block + i, 1) < 0) {
            return -1;
        }

        if (dest != 0) {
            return 0;
        }

        addr += 1;
    }

    return 1;
}


int set_availability(fs_descriptor *fsdesc, int block, const uint8_t *value) {
    uintptr_t addr = fsdesc->avail_block_table_offset + block;

    if (fsdesc->fsdw((uintptr_t) value, addr, 1) < 0) {
        return -1;
    }
    
    return 0;
}

int copy_block(fs_descriptor *fsdesc, int src, int dest) {
    uint8_t buf[BLOCK_SIZE];

    uintptr_t src_addr = fsdesc->data_offset + src * BLOCK_SIZE;
    uintptr_t dest_addr = fsdesc->data_offset + dest * BLOCK_SIZE;

    if (fsdesc->fsdr((uintptr_t) buf, src_addr, BLOCK_SIZE) < 0) {
        return -1;
    }

    if (fsdesc->fsdw((uintptr_t) buf, dest_addr, BLOCK_SIZE) < 0) {
        return -1;
    }

    return 0;
}



int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr, fs_disk_writer fsdw) {
    fsdesc->fsdr = fsdr;
    fsdesc->fsdw = fsdw;

    int r = fsdr((uintptr_t) &fsdesc->metadata, 0, METADATA_SIZE);
    if (r < 0) {
        return r;
    }

    fsdesc->inode_table_offset = METADATA_SIZE;
    fsdesc->block_usage_offset = fsdesc->inode_table_offset + fsdesc->metadata.inode_count * INODE_ENTRY_SIZE;
    fsdesc->tree_usage_offset = fsdesc->block_usage_offset + fsdesc->metadata.inode_count;
    fsdesc->tree_offset = fsdesc->tree_usage_offset + fsdesc->metadata.block_count * BLOCK_SIZE;
    fsdesc->data_offset = fsdesc->tree_offset + fsdesc->metadata.node_count * NODE_SIZE;

    return 0;
}

int fs_read(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, off_t offset) {
    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;

    struct fs_inode_entry entry;

    if (fsdesc->fsdr((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE) < 0)
        return -1;

    uintptr_t start_addr = fsdesc->data_offset + entry.start_block * BLOCK_SIZE + offset;

    if (fsdesc->fsdr((uintptr_t) buf, start_addr, size) < 0)
        return -1;

    return 0;
}

int fs_write(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, off_t offset) {
    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;

    struct fs_inode_entry entry;

    if (fsdesc->fsdr((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE) < 0)
        return -1;

    uint32_t total_block = SIZE_TO_BLOCK(offset+size);
    if (total_block > entry.block_count) {
        int new_block = total_block - entry.block_count;
        
        if (are_blocks_avaiblable(fsdesc, entry.start_block + entry.block_count, new_block)) {
            for (int i = 0; i < new_block; i++) {
                set_availability(fsdesc, entry.start_block + entry.block_count + i, &ONE);
            }
        } else {
            int64_t r = search_free_blocks(fsdesc, total_block);
            if (r < 0) {
                return -1;
            }

            uint32_t new_start_block = (uint32_t) r;

            for (uint32_t i = 0; i < entry.block_count; i++) {
                copy_block(fsdesc, entry.start_block + i, new_start_block + i);
                set_availability(fsdesc, entry.start_block + i, &ZERO);
                set_availability(fsdesc, new_start_block + i, &ONE);
            }

            entry.start_block = new_start_block;
        }

        entry.block_count = total_block;
        fsdesc->fsdw((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
    }

    uintptr_t start_addr = fsdesc->data_offset + entry.start_block * BLOCK_SIZE + offset;
    if (fsdesc->fsdw((uintptr_t) buf, start_addr, size) < 0)
        return -1;
    
    return 0;
}

int follow_node(fs_descriptor *fsdesc, fs_node_t* src_node, const char *edge, fs_node_t* dst_node) {
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (strcmp(src_node->children[i].name, edge) == 0) {
            uintptr_t node_addr = fsdesc->tree_offset + src_node->children[i].index * NODE_SIZE;

            if (fsdesc->fsdr((uintptr_t) dst_node, node_addr, NODE_SIZE) < 0)
                return -1;

            return 0;
        }
    }

    return -1;
}

int search_node(fs_descriptor *fsdesc, const char *path, fs_node_t *node) {
    if (path[0] != '\\')
        return -1;

    if (fsdesc->fsdr((uintptr_t) node, fsdesc->tree_offset, NODE_SIZE) < 0)
        return -1;

    while (*path != '\0') {
        if (*path == '\\')
            path++;
        
        char name[NAME_SIZE];
        int i = 0;
        while (*path != '\0' && *path != '\\') {
            name[i] = *path;
            i++;

            if (i == NAME_SIZE - 1)
                return -1;
            
            path++;
        }
        name[i] = '\0';

        if (follow_node(fsdesc, node, name, node) < 0)
            return -1;
    }

    return 0;
}

int fs_getattr(fs_descriptor *fsdesc, const char *path) {
    fs_node_t node;
    if (search_node(fsdesc, path, &node) < 0)
        return -1;

    return node.value;
}

int fs_readdir_init(fs_descriptor *fsdesc, const char *path, fs_dirreader *dr) {
    fs_node_t node;
    int64_t r = search_node(fsdesc, path, &node);
    if (r < 0)
        return -1;
    uint32_t node_index = (uint32_t) r;

    dr->fsdesc = fsdesc;
    dr->node_index = node_index;
    dr->offset = 0;

    return node.children_count;
}

int fs_readdir_next(fs_dirreader *dr, char *buffer) {
    fs_node_t node;
    if (dr->fsdesc->fsdr((uintptr_t) &node, dr->fsdesc->tree_offset + dr->node_index * NODE_SIZE, NODE_SIZE) < 0)
        return -1;
    
    strcpy(buffer, node.children[dr->offset].name);
    dr->offset++;

    return 0;
}


int64_t search_available_node(fs_descriptor *fsdesc) {
    for (uint32_t i = 0; i < fsdesc->metadata.node_count; i++) {
        uint8_t used;

        if (fsdesc->fsdr((uintptr_t) &used, fsdesc->tree_usage_offset + i, 1) < 0)
            return -1;

        if (used == 0)
            return i;
    }

    return -1;
}




int fs_touch(fs_descriptor *fsdesc, const char *parent_path, const char *name, int value) {
    /*int parent_path_size = 0;
    int name_size = 0;
    
    char *cur = path;
    while (*cur) {
        if (*cur == '\\') {
            parent_path_size += name_size + 1;
            name_size = 0;
        }

        cur++;
    }

    if (name_size == 0)
        return -1;*/

    
    fs_node_t node;
    
    int64_t r = search_node(fsdesc, parent_path, &node);
    if (r < 0)
        return -1;
    uint32_t parent_node_index = (uint32_t) r;
    
    if (node.children_count == MAX_CHILDREN)
        return -1;

    int i = 0;
    while (node.children[i].index == 0) {
        i += 1;
        
        if (i == MAX_CHILDREN)
            return -1;
    }


    r = search_available_node(fsdesc);
    if (r < 0)
        return -1;
    uint32_t child_node_index = (uint32_t) r;


    strcpy(node.children[i].name, name);
    node.children[i].index = child_node_index;
    fsdesc->fsdw((uintptr_t) &node, fsdesc->tree_offset + parent_node_index, NODE_SIZE);

    
    memset(&node, 0, NODE_SIZE);
    node.value = value;
    fsdesc->fsdw((uintptr_t) &ONE, fsdesc->tree_usage_offset + child_node_index, 1);
    fsdesc->fsdw((uintptr_t) &node, fsdesc->tree_offset + child_node_index, NODE_SIZE);

    return 0;
}


//TODO: coder l'arbre
int fs_truncate(fs_descriptor *fsdesc, fs_ino ino, off_t size) {
    // Validate inputs
    if (fsdesc == NULL || size < 0) {
        return -1;
    }

    // Read inode entry
    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;
    struct fs_inode_entry entry;

    int r = fsdesc->fsdr((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
    if (r < 0) {
        return r; // Disk read error
    }

    // Calculate the number of blocks needed for the new size
    uint32_t new_block_count = SIZE_TO_BLOCK(size);

    // Check if we need to allocate new blocks
    if (new_block_count > entry.block_count) {
        int free_blocks = search_free_blocks(fsdesc, new_block_count - entry.block_count);
        if (free_blocks < 0) {
            return -1; // No free blocks available
        }

        // Update inode entry with new block count and start block
        entry.start_block = free_blocks;
        entry.block_count = new_block_count;

        // Write updated inode entry back to disk
        r = fsdesc->fsdw((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
        if (r < 0) {
            return r; // Disk write error
        }
    } else {
        // If we are reducing the size, just update the block count
        entry.block_count = new_block_count;

        // Write updated inode entry back to disk
        r = fsdesc->fsdw((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
        if (r < 0) {
            return r; // Disk write error
        }
    }

    return 0; // Success
}