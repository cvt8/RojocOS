#include "string.h"



void split_path(normpath path, normpath *parent_path, string *child_name) {
    int last_slash = -1;
    for (size_t i = 0; i < path.len; i++) {
        if (path.str[i] == '/')
            last_slash = i;
    }
    assert(last_slash != -1);

    parent_path->str = path.str;
    if (last_slash == 0)
        parent_path->len = 1;
    else
        parent_path->len = last_slash;
    
    child_name->str = path.str + last_slash + 1;
    child_name->len = path.len - last_slash - 1;
}

void copy_to_buffer(char *buffer, string str) {
    for (size_t i = 0; i < str.len; i++) {
        buffer[i] = str.str[i];
    }
    buffer[str.len] = '\0';
}

int equal_to_buffer(char *buffer, string str) {
    for (size_t i = 0; i < str.len; i++) {
        if (buffer[i] != str.str[i])
            return 0;
    }

    if (buffer[str.len] != '\0')
        return 0;

    return 1;
}


