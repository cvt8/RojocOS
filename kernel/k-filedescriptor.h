#ifndef WEENSYOS_K_FILEDESCRIPTOR_H
#define WEENSYOS_K_FILEDESCRIPTOR_H

#include "kernel.h"

int fdlist_add_entry(proc_fdlist_t *fdl, int fd, int inode);
int fdlist_remove_entry(proc_fdlist_t *fdl, int fd);
proc_fdentry_t* fdlist_search_entry(proc_fdlist_t *fdl, int fd);

#endif
