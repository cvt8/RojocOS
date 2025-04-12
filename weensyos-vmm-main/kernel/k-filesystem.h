#include "lib.h"


struct inode_list_element;
typedef struct inode_list_element* inode_list;

struct inode_list_element {
    int fd;
    int inode;
    inode_list queue;
};

// -1 si pas trouvé
// inode >= 0 si trouvé
int get_inode(inode_list ilst, int fd);