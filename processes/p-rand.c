#include "process.h"
#include "lib.h"

//  affiche rand 
void process_main(int argc, char* argv[]) {

    for (int i = 1; i < argc; i++) {
        app_printf(0, "%s ", argv[i]);
    }

    app_printf(0, "\n");


    app_printf(0, "%d\n ", rand());


    sys_exit(0);
}

