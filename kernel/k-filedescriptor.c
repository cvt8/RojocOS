#include "kernel.h"
#include "k-malloc.h"


int fdlist_add_entry(proc_fdlist_t *fdl, int fd, int inode) {
    while (*fdl) {
        *fdl = (*fdl)->next;
    }

    proc_fdentry_t *entry = (proc_fdentry_t *) kernel_malloc(sizeof(proc_fdentry_t));

    if (entry == NULL) {
        return -1;
    }

    entry->fd = fd;
    entry->inode = inode;
    entry->offset = 0;
    entry->next = NULL;

    *fdl = entry;
    return 0;
}

int fdlist_get_inode(proc_fdlist_t *fdl, int fd) {
    while (*fdl) {
        if ((*fdl)->fd == fd) {
            return (*fdl)->inode; 
        }
        
        *fdl = (*fdl)->next;
    }

    return -1;
}

proc_fdentry_t* fdlist_search_entry(proc_fdlist_t *fdl, int fd) {
    while (*fdl) {
        if ((*fdl)->fd == fd) {
            return *fdl; 
        }
        
        *fdl = (*fdl)->next;
    }

    return NULL;
}

int fdlist_remove_entry(proc_fdlist_t *fdl, int fd) {
    while (*fdl) {
        if ((*fdl)->fd == fd) {
            *fdl = (*fdl)->next;
            return 0;
        }
        
        *fdl = (*fdl)->next;
    }
    
    return -1;
}
