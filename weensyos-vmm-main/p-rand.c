#include "process.h"
#include "lib.h"
#define ALLOC_SLOWDOWN 100

extern uint8_t end[];

// These global variables go on the data page.
uint8_t* heap_top;
uint8_t* stack_bottom;

void process_main(void) {

    pid_t p = sys_getpid();
    srand(p);

    for (size_t i = 0; i < 10; i++) {
        app_printf(p, "%d\n", rand());
    }

    while (1) {
        sys_yield();
    }
}
