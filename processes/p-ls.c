
#include "process.h"
#include "lib.h"


void process_main() {

    char buffer[64];

    int r = sys_listdir("/", buffer);
    assert(r >= 0);

    app_printf(0, "%s", buffer);

    sys_exit(0);
}

