#include "process.h"
#include "lib.h"



const char *usage_str =
    "Usage: touch [FILE]\n"
    "Touch a file.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(0, "%s", usage_str);
    sys_exit(1);
}


void process_main(int argc, char* argv[]) {
    if (argc <= 1) usage();

    int r = sys_touch(argv[1]);
    if (r < 0) handle_error(-r);

    sys_exit(0);
}
