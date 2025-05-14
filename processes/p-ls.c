
#include "process.h"
#include "lib.h"


void process_main() {
    pid_t p = sys_getpid();
    
    app_printf(3, "Hello, from process %d\n", p);
    app_printf(3, "num : %d \n", 5);

    sys_exit(0);
}

