#include "process.h"

void process_main(int argc, char* argv[]) {

    for (int i = 1; i < argc; i++) {
        app_printf(0, "%s ", argv[i]);
    }

    assert(0);

    app_printf(0, "\n");
    sys_exit(0);
}

