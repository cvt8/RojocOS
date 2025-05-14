#include "k-filesystem.h"

int get_inode(inode_list ilst, int fd) {
    if (ilst == NULL || ilst->fd > fd) { 
        return -1;
    }
    if (ilst->fd == fd) {
        return fd;
    }
    return get_inode(ilst->queue, fd);
}

int add_entry(inode_list ilst, int inode) {
    int fd = 0;
    return 0;
}
