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

    int r = sys_mkdir(argv[1]);
    if (r < 0) handle_error(-r);

    sys_exit(0);
}
