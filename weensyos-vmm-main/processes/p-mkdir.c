#include "process.h"
#include "lib.h"
#include "lib-malloc.h"

#define BUFFER_SIZE 256


const char *usage_str =
    "Usage: mkdir [FILE]...\n"
    "Concatenate FILE(s) to standard output.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(0, "%s", usage_str);
    sys_exit(1);
}


void process_main(int argc, char* argv[]) {

    /*app_printf(0, "argc: %d\n", argc);
    app_printf(0, "argv: %p\n", argv);
    app_printf(0, "argv[0]: %p\n", argv[0]);
    app_printf(0, "argv[0]: %s\n", argv[0]);

    pid_t p = sys_getpid();
    srand(p);

    int* nums = (int*) malloc(10);

    for (int i = 0; i < 10; i++) {
        nums[i] = rand();
    }
    for (int i = 0; i < 10; i++) {
        app_printf(p, "%d\n", nums[i]);
    }*/

    if (argc <= 1) {
        usage();
    }

    for (int i = 1; i < argc; i++) {
        char *pathname = argv[i];
        int fd = sys_open(pathname);
        assert(fd >= 0);

        char *buf = (char *) malloc(BUFFER_SIZE);
        buf[BUFFER_SIZE-1] = '\0';
    
        int r = sys_read(fd, (void *) buf, BUFFER_SIZE-1);
        assert(r >= 0);

        app_printf(4, "%s", buf);
    }

    sys_exit(0);
}
