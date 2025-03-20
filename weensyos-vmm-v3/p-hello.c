#include "process.h"
#include "lib.h"
#define SLOWDOWN 100

extern uint8_t end[];

// These global variables go on the data page.
uint8_t* heap_top;
uint8_t* stack_bottom;


char scan_char(void) {
    int c;

    do {
        c = sys_read();
        sys_yield();
    } while (c == -1);

    return c;
}

void scan_line(char* dst) {
    int length = 0;

    while (1) {
        int c = scan_char();
        app_printf(0, "%c", c);

        if (c == '\b') {
            if (length != 0)
                length--;
        } else if (c == '\n') {
            dst[length] = 0;
            return;
        } else {
            dst[length++] = c;
        }
    }
}

void process_main(void) {
    pid_t p = sys_getpid();
    srand(p);

    assert(p == 5);

    app_printf(p, "Hello, from process %d\n", p);

    app_printf(p, "Shell\n");

    while (1) {
        app_printf(p, "> ");

        char line[81];
        scan_line(line);

        
        app_printf(p, "< %s\n", line);

        if (strcmp(line, "run") == 0) {
            app_printf(p, "Running...\n");
            sys_exec("rand");
        }

        
    }

    while (1) {
        char c = scan_char();

        app_printf(p, "%c", c);
    

        sys_yield();
    }
}
