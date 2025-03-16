#include "process.h"
#include "lib.h"
#define SLOWDOWN 100

extern uint8_t end[];

// These global variables go on the data page.
uint8_t* heap_top;
uint8_t* stack_bottom;

void process_main(void) {
    pid_t p = sys_getpid();
    srand(p);

    assert(p == 5);

    app_printf(p, "Hello, from process %d\n", p);

    while (1) {
        if (rand() % SLOWDOWN == 0) {
            app_printf(p, "%d\n", rand() % 100);
            sys_hello();
        }

        sys_yield();
    }
}
