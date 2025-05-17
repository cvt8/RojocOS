#include "process.h"
#include "lib.h"


const char *usage_str =
    "Usage: rm [FILE]\n"
    "Remove a file.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(1, "%s", usage_str);
    sys_exit(1);
}


void process_main(int argc, char* argv[]) {
    if (argc <= 1) {
        usage();
    }

    int r = sys_remove(argv[1]);
    assert(r == 0);

    sys_exit(0);
}
