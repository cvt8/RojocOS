#include "process.h"
#include "lib.h"
#include "lib-malloc.h"

#define BUFFER_SIZE 256


const char *usage_str =
    "Usage: cat [FILE]...\n"
    "Concatenate FILE(s) to standard output.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(0, "%s", usage_str);
    sys_exit(1);
}


void process_main(int argc, char* argv[]) {
    if (argc <= 1) usage();
    
    int read_count;

    if (argc == 2) {
        read_count = 1024;
        argc++;
    } else {
        int r = string_to_char(argv[argc-1], &read_count);
        if (r < 0) usage();
        if (read_count == 0) usage();
    }

    //app_printf(0, "read_count : %d\n", read_count);

    for (int i = 1; i < argc-1; i++) {
        char *pathname = argv[i];
        int fd = sys_open(pathname);
        if (fd < 0) handle_error(-fd);

        char *buf = (char *) malloc(read_count+1);
    
        int r = sys_read(fd, (void *) buf, read_count);
        if (r < 0) handle_error(-r);
        buf[r] = '\0';

        app_printf(4, "%s", buf);
    }

    sys_exit(0);
}
