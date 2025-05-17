#ifndef __STRING_H__
#define __STRING_H__

#include "lib.h"

typedef struct {
    const char *str;
    size_t len;
} string;


// A normpath is a normalized absolute path with these properties:
// - Begins with a single '/' (absolute path)
// - Does not contain "/../" or "/./" segments
// - Does not contain repeated slashes ("//")
// - Does not end with a slash, unless the path is exactly "/"
// Example:
//   - "/" is valid
//   - "/foo/bar" is valid
//   - "/foo//bar", "/foo/./bar", "/foo/../bar", "/foo/bar/" are invalid
typedef string normpath;

void split_path(normpath path, normpath *parent_path, string *child_name);

void copy_to_buffer(char *buffer, string str);

int equal_to_buffer(char *buffer, string str);

#endif /* __STRING_H__ */