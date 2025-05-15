#include "process.h"
#include "lib.h"
#include "lib-malloc.h"

#define BUFFER_SIZE 256


const char *usage_str =
    "Usage: mkdir [FILE]\n"
    "Make directory.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(0, "%s", usage_str);
    sys_exit(1);
}


void process_main(int argc, char* argv[]) {

    
    if (argc <= 1) {
        usage();
    }

    app_printf(0, "mkdir %s\n", argv[1]);

    int r = sys_mkdir(argv[1]);
    assert(r >= 0);

    sys_exit(0);
}
