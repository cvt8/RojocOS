#include "process.h"
#include "lib.h"

void process_main(void) {
    for (int i = 0; ; ++i) {
        unsigned v = sys_getrandom();          // kernel random number generator (RNG)
        app_printf(0, "%d: %u\n", i, v);
         if (i % 50 == 49)
             sys_yield();
    }
}
