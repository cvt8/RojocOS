#include "aes.h"
#include "errno.h"
#include "filesystem.h"
#include "string.h"
#include "kernel.h"

#define METADATA_SIZE sizeof(fs_metadata)
#define BLOCK_SIZE 4096

// Pour nombres positifs uniquement
#define SIZE_TO_BLOCK(x) ((uint32_t) (((x) + BLOCK_SIZE - 1) / BLOCK_SIZE))


typedef struct fs_inode_entry {
    uint8_t ref;
    uint64_t size;
    uint32_t start_block;
    uint32_t block_count;
    uint8_t cipher_key[FS_KEY_SIZE];
    uint8_t cipher_iv[FS_IV_SIZE];
} fs_inode_entry;

#define NAME_SIZE 32
#define MAX_CHILDREN 32

typedef struct fd_node_child {
    char name[NAME_SIZE];
    uint32_t index;
} fd_node_child_t;

typedef struct fs_node {
    uint32_t value;
    int children_count;
    fd_node_child_t children[MAX_CHILDREN];
} fs_node_t;


#define NODE_SIZE sizeof(fs_node_t)


#define INODE_ENTRY_SIZE sizeof(struct fs_inode_entry)


static const uint8_t ZERO = 0;
static const uint8_t ONE = 1;



int decrypt_block(fs_descriptor *fsdesc, uint32_t index, struct AES_ctx *ctx, uint8_t *buffer) {
    uintptr_t addr = fsdesc->data_offset + index * BLOCK_SIZE;
    int r = fsdesc->fsdr((uintptr_t) buffer, addr, BLOCK_SIZE);
    if (r < 0) return r;

    AES_CTR_xcrypt_buffer(ctx, buffer, BLOCK_SIZE);

    return 0;
}

int encrypt_block(fs_descriptor *fsdesc, uint32_t index, struct AES_ctx *ctx, uint8_t *buffer) {
    AES_CTR_xcrypt_buffer(ctx, buffer, BLOCK_SIZE);

    uintptr_t addr = fsdesc->data_offset + index * BLOCK_SIZE;
    int r = fsdesc->fsdw((uintptr_t) buffer, addr, BLOCK_SIZE);
    if (r < 0) return r;

    r = fsdesc->fsdw(&ONE, fsdesc->block_usage_offset + index, 1);
    if (r < 0) return r;

    return 0;
}

int64_t search_available_inode(fs_descriptor *fsdesc) {
    for (uint32_t i = 1; i < fsdesc->metadata.inode_count; i++) {
        uint8_t used;
        uint64_t addr = fsdesc->inode_table_offset + i*INODE_ENTRY_SIZE;

        int r = fsdesc->fsdr((uintptr_t) &used, addr, 1);
        if (r < 0) return r;
        
        if (!used) return i;
    }

    return -ENOSPC;
}

int64_t fs_alloc_inode(fs_descriptor *fsdesc) {
    int64_t r = search_available_inode(fsdesc);
    if (r < 0) return r;
    uint32_t inode = (uint32_t) r;

    fs_inode_entry entry;
    r = fsdesc->fsdr((uintptr_t) &entry, fsdesc->inode_table_offset + inode*INODE_ENTRY_SIZE, INODE_ENTRY_SIZE);
    if (r < 0) return r;
    
    entry.ref += 1;

    r = fsdesc->fsdw((uintptr_t) &entry, fsdesc->inode_table_offset + inode*INODE_ENTRY_SIZE, INODE_ENTRY_SIZE);
    if (r < 0) return r;
    
    return inode;
}


int64_t search_free_blocks(fs_descriptor *fsdesc, uint32_t n) {
    uint32_t start_block = 0;
    uint32_t count = 0;
    uintptr_t addr = fsdesc->avail_block_table_offset;

    while (start_block+count < fsdesc->metadata.block_count && count < n) {
        unsigned char dest;

        int r = fsdesc->fsdr((uintptr_t) &dest, addr, 1);
        if (r < 0) return r;

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
    
    return -ENOSPC;
}

int are_blocks_avaiblable(fs_descriptor *fsdesc, int start_block, int n) {
    uintptr_t addr = fsdesc->avail_block_table_offset;

    for (int i = 0; i < n; i++) {
        unsigned char dest;

        int r = fsdesc->fsdr((uintptr_t) &dest, addr + start_block + i, 1);
        if (r < 0) return r;

        if (dest != 0) {
            return 0;
        }

        addr += 1;
    }

    return 1;
}


int set_availability(fs_descriptor *fsdesc, int block, const uint8_t *value) {
    uintptr_t addr = fsdesc->avail_block_table_offset + block;

    int r = fsdesc->fsdw((uintptr_t) value, addr, 1);
    if (r < 0) return r;
    
    return 0;
}

int copy_block(fs_descriptor *fsdesc,
        uint32_t src_index, uint8_t *src_key, uint8_t *src_iv,
        uint32_t dst_index, uint8_t *dst_key, uint8_t *dst_iv,
        uint32_t n) {

    struct AES_ctx ctx_dec;
    AES_init_ctx_iv(&ctx_dec, src_key, src_iv);

    struct AES_ctx ctx_enc;
    AES_init_ctx_iv(&ctx_enc, dst_key, dst_iv);

    uint8_t buf[BLOCK_SIZE];

    for (int i = 0; i < n; i++) {
        int r = decrypt_block(fsdesc, src_index + i, &ctx_dec, buf);
        if (r < 0) return r;
        r = encrypt_block(fsdesc, dst_index + i, &ctx_enc, buf);
        if (r < 0) return r;
    }

    return 0;
}



int fs_init(fs_descriptor *fsdesc, fs_disk_reader fsdr, fs_disk_writer fsdw, fs_random_generator fsrng) {
    fsdesc->fsdr = fsdr;
    fsdesc->fsdw = fsdw;
    fsdesc->fsrng = fsrng;

    int r = fsdr((uintptr_t) &fsdesc->metadata, 0, METADATA_SIZE);
    if (r < 0) return r;

    fsdesc->metadata.block_count = 16;
    fsdesc->metadata.inode_count = 16;
    fsdesc->metadata.node_count = 16;

    fsdesc->inode_table_offset = METADATA_SIZE;
    fsdesc->block_usage_offset = fsdesc->inode_table_offset + fsdesc->metadata.inode_count * INODE_ENTRY_SIZE;
    fsdesc->tree_usage_offset = fsdesc->block_usage_offset + fsdesc->metadata.inode_count;
    fsdesc->tree_offset = fsdesc->tree_usage_offset + fsdesc->metadata.block_count * BLOCK_SIZE;
    fsdesc->data_offset = fsdesc->tree_offset + fsdesc->metadata.node_count * NODE_SIZE;

    return 0;
}

ssize_t fs_read(fs_descriptor *fsdesc, fs_ino ino, void *buf, size_t size, off_t offset) {
    if (size > FS_IO_MAX_SIZE)
        return -EINVAL;


    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;

    struct fs_inode_entry entry;
    int r = fsdesc->fsdr((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
    if (r < 0) return r;

    if (size + offset > entry.size)
        size = entry.size - offset;

    log_printf("fs_read / entry.start_block : %d\n", entry.start_block);

    uintptr_t start_addr = fsdesc->data_offset + entry.start_block * BLOCK_SIZE + offset;

    r = fsdesc->fsdr((uintptr_t) buf, start_addr, size);
    if (r < 0) return r;

    return size;
}

ssize_t fs_write(fs_descriptor *fsdesc, fs_ino ino, const void *buf, size_t size, off_t offset) {
    if (size > FS_IO_MAX_SIZE)
        return -EINVAL;

    uintptr_t entry_addr = METADATA_SIZE + ino * INODE_ENTRY_SIZE;
    struct fs_inode_entry entry;
    int64_t r = fsdesc->fsdr((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
    if (r < 0) return r;

    if (entry.size < offset)
        return -EINVAL;
    
    uint32_t total_block = SIZE_TO_BLOCK(offset+size); // Total number of blocks needed to write the file
    int new_block = total_block - entry.block_count;
    int aba = -1;

    // If we need more blocks than the file already has, we need to allocate new blocks
    if (new_block > 0) {
        aba = are_blocks_avaiblable(fsdesc, entry.start_block + entry.block_count, new_block);
        if (aba < 0) return aba;
    }

    uint8_t partial_block[BLOCK_SIZE];
    if (offset % BLOCK_SIZE != 0) {
        // If the offset is not a multiple of the block size, we need to handle the partial block
        struct AES_ctx ctx;
        AES_init_ctx_iv(&ctx, entry.cipher_key, entry.cipher_iv); //TODO: IV
        r = decrypt_block(fsdesc, entry.start_block + offset / BLOCK_SIZE, &ctx, partial_block);
        if (r < 0) return r;

        memcpy(partial_block + offset % BLOCK_SIZE, buf, BLOCK_SIZE - offset % BLOCK_SIZE);
    }

    if (aba == 0) {
        // If the blocks right after the last block of the file are not available, we need to search for free blocks
        r = search_free_blocks(fsdesc, total_block);
        if (r < 0) return r;

        uint32_t new_start_block = (uint32_t) r;
        uint8_t dst_key[FS_KEY_SIZE];
        uint8_t dst_iv[FS_IV_SIZE];
        fsdesc->fsrng(dst_key, FS_KEY_SIZE);
        fsdesc->fsrng(dst_iv, FS_IV_SIZE);

        copy_block(fsdesc,
            entry.start_block, entry.cipher_key, entry.cipher_iv,
            new_start_block, dst_key, dst_iv,
            offset / BLOCK_SIZE);

        entry.start_block = new_start_block;
        memcpy(entry.cipher_key, dst_key, FS_KEY_SIZE);
        memcpy(entry.cipher_iv, dst_iv, FS_IV_SIZE);
    }

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, entry.cipher_key, entry.cipher_iv + offset / BLOCK_SIZE); //TODO: IV
    
    // Handle partial block
    if (offset % BLOCK_SIZE != 0) {
        r = encrypt_block(fsdesc, entry.start_block + offset / BLOCK_SIZE, &ctx, partial_block);
        if (r < 0) return r;
    }

    // Handle the rest of the blocks
    if (new_block) {
        int final_blocks_index = entry.start_block + SIZE_TO_BLOCK(offset);
        uintptr_t final_blocks_addr = fsdesc->data_offset + final_blocks_index * BLOCK_SIZE;

        for (int i = 0; i < size - (BLOCK_SIZE - offset % BLOCK_SIZE); i++) {
            encrypt_block(fsdesc, final_blocks_index + i, &ctx, buf);
            buf += BLOCK_SIZE;
        }

        entry.block_count = total_block;
    }

    entry.size = offset + size;
    r = fsdesc->fsdw((uintptr_t) &entry, entry_addr, INODE_ENTRY_SIZE);
    if (r < 0) return r;

    if (aba == 0) {
        for (int i = 0; i < total_block - new_block; i++) {
            r = set_availability(fsdesc, entry.start_block + i, &ZERO);
            if (r < 0) return r;
        }
    }
    
    return size;
}

// Returns a negative value on error. On success, returns the index of the found node and copies it to *dst_node.
// src_node can be dst_node
int64_t follow_node(fs_descriptor *fsdesc, fs_node_t* src_node, const char *edge, fs_node_t* dst_node) {
    log_printf("follow_node / src_node->children_count : %d\n", src_node->children_count);
    log_printf("follow_node / edge : %s\n", edge);


    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (strcmp(src_node->children[i].name, edge) == 0) {
            log_printf("follow_node / node name : %s\n", src_node->children[i].name);
            log_printf("follow_node / node index : %d\n", src_node->children[i].index);

            uint32_t node_index = src_node->children[i].index;
            uintptr_t node_addr = fsdesc->tree_offset + node_index * NODE_SIZE;

            int r = fsdesc->fsdr((uintptr_t) dst_node, node_addr, NODE_SIZE);
            if (r < 0) return r;

            return node_index;
        }
    }

    return -ENOENT;
}


// Returns a negative value on error. On success, returns the index of the found node and copies it to *node.
int64_t search_node(fs_descriptor *fsdesc, normpath path, fs_node_t *node) {
    assert(path.str[0] == '/');

    log_printf("search_node / path : %.*s\n", (int)path.len, path.str);

    int64_t r = fsdesc->fsdr((uintptr_t) node, fsdesc->tree_offset, NODE_SIZE);
    if (r < 0) return r;

    uint32_t node_index = 0;

    while (path.len > 0) {
        if (path.str[0] == '/') {
            path.str++;
            path.len--;
        }
        
        char name[NAME_SIZE];
        int i = 0;
        while (path.len > 0 && path.str[0] != '/') {
            name[i] = path.str[0];
            i++;

            if (i == NAME_SIZE - 1)
                return -ENAMETOOLONG;
            
            path.str++;
            path.len--;
        }
        
        name[i] = '\0';

        r = follow_node(fsdesc, node, name, node);
        if (r < 0) return r;

        node_index = (uint32_t) r;
    }

    return node_index;
}

int64_t fs_getattr(fs_descriptor *fsdesc, normpath path) {
    fs_node_t node;

    int64_t r = search_node(fsdesc, path, &node);
    if (r < 0) return r;

    return node.value;
}

int fs_readdir_init(fs_descriptor *fsdesc, normpath path, fs_dirreader *dr) {
    
    fs_node_t node;
    int64_t r = search_node(fsdesc, path, &node);
    if (r < 0) return r;
    uint32_t node_index = (uint32_t) r;

    
    log_printf("fs_readdir_init / path.str(%d) : %.*s\n", (int)path.len, (int)path.len, path.str);
    log_printf("fs_readdir_init / node : %d\n", node_index);

    dr->fsdesc = fsdesc;
    dr->node_index = node_index;
    dr->offset = 0;

    return node.children_count;
}

int fs_readdir_next(fs_dirreader *dr, char *buffer) {
    fs_node_t node;
    int64_t r = dr->fsdesc->fsdr((uintptr_t) &node, dr->fsdesc->tree_offset + dr->node_index * NODE_SIZE, NODE_SIZE);
    if (r < 0) return r;
    
    strcpy(buffer, node.children[dr->offset].name);
    dr->offset++;

    return 0;
}


int64_t search_available_node(fs_descriptor *fsdesc) {
    for (uint32_t i = 1; i < fsdesc->metadata.node_count; i++) { // 0 is root
        uint8_t used;

        int r = fsdesc->fsdr((uintptr_t) &used, fsdesc->tree_usage_offset + i, 1);
        if (r < 0) return r;

        if (used == 0)
            return i;
    }

    return -ENOSPC;
}

int fs_touch(fs_descriptor *fsdesc, string path, uint32_t value) {
    string parent_path;
    string child_name;
    split_path(path, &parent_path, &child_name);

    log_printf("fs_touch / parent_path : %.*s\n", (int)parent_path.len, parent_path.str);
    log_printf("fs_touch / child_name : %.*s\n", (int)child_name.len, child_name.str);

    assert(child_name.len < NAME_SIZE);

    fs_node_t node;
    int64_t r = search_node(fsdesc, parent_path, &node);
    if (r < 0) return r;
    uint32_t parent_node_index = (uint32_t) r;

    log_printf("fs_touch / parent_node_index : %d\n", parent_node_index);

    for (int i = 0; i < node.children_count; i++) {
        if (node.children[i].index && equal_to_buffer(node.children[i].name, child_name))
            return -EEXIST;
    }
    
    if (node.children_count == MAX_CHILDREN)
        return -ENOSPC;

    r = search_available_node(fsdesc);
    if (r < 0) return r;
    uint32_t child_node_index = (uint32_t) r;

    log_printf("fs_touch / child_node_index : %d\n", child_node_index);
    
    copy_to_buffer(node.children[node.children_count].name, child_name);
    node.children[node.children_count].index = child_node_index;
    node.children_count += 1;
    fsdesc->fsdw((uintptr_t) &node, fsdesc->tree_offset + parent_node_index * NODE_SIZE, NODE_SIZE);

    
    memset(&node, 0, NODE_SIZE);
    node.value = value;
    fsdesc->fsdw((uintptr_t) &ONE, fsdesc->tree_usage_offset + child_node_index, 1);
    fsdesc->fsdw((uintptr_t) &node, fsdesc->tree_offset + child_node_index * NODE_SIZE, NODE_SIZE);

    return 0;
}


//TODO: coder l'arbre
int fs_truncate(fs_descriptor *fsdesc, fs_ino ino, off_t size) {
    return -9;
}

int fs_remove(fs_descriptor *fsdesc, normpath path) {
    normpath parent_path;
    string child_name;
    split_path(path, &parent_path, &child_name);

    fs_node_t node;
    int64_t r = search_node(fsdesc, parent_path, &node);
    if (r < 0) return r;
    uint32_t parent_node_index = (uint32_t) r;

    uint32_t child_node_index = 0;

    for (int i = 0; i < node.children_count; i++) {
        if (node.children[i].index && equal_to_buffer(node.children[i].name, child_name)) {
            child_node_index = node.children[i].index;
            assert(child_node_index);

            node.children[i].index = 0;
            node.children[i].name[0] = '\0';
            node.children[i].index = node.children[node.children_count-1].index;
            memcpy(node.children[i].name, node.children[node.children_count-1].name, NAME_SIZE);
            node.children_count -= 1;
            break;
        }
    }

    if (!child_node_index)
        return -ENOENT;

    r = fsdesc->fsdw((uintptr_t) &node, fsdesc->tree_offset + parent_node_index * NODE_SIZE, NODE_SIZE);
    if (r < 0) return r;

    memset(&node, 0, NODE_SIZE);
    r = fsdesc->fsdr((uintptr_t) &node, fsdesc->tree_offset + child_node_index * NODE_SIZE, NODE_SIZE);
    if (r < 0) return r;

    r = fsdesc->fsdw((uintptr_t) &ZERO, fsdesc->tree_usage_offset + child_node_index, 1);
    if (r < 0) return r;
    
    /*for (int i = 0; i < node.block_count; i++) {
        fsdesc->fsdw((uintptr_t) &ZERO, fsdesc->data_offset + node.start_block + i * BLOCK_SIZE, BLOCK_SIZE);
    }*/

    return 0;
}
