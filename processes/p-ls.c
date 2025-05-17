
#include "process.h"
#include "lib.h"

/*const char *usage_str =
    "Usage: mkdir [FILE]\n"
    "Make directory.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(0, "%s", usage_str);
    sys_exit(1);
}*/


void process_main(int argc, char* argv[]) {

    char *path;

    if (argc == 1) {
        path = ".";
    } else {
        path = argv[1];
    }

    char buffer[64];

    int r = sys_listdir(path, buffer);
    if (r < 0)
        handle_error(-r);

    app_printf(0, "%s", buffer);

    sys_exit(0);
}

