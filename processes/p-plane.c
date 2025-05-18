#include "process.h"
#include "lib.h"

#define TEXT_SIZE 64


const char *usage_str =
    "Usage: plane [FILE]\n"
    "Plane a file.\n";


void __attribute__((noreturn)) usage(void) {
    app_printf(1, "%s", usage_str);
    sys_exit(1);
}


void process_main(int argc, char* argv[]) {
    if (argc <= 1) {
        usage();
    }

    char buffer[TEXT_SIZE];
    unsigned int length = scan_line(buffer, TEXT_SIZE-2);
    buffer[length] = '\n';
    buffer[length+1] = '\0';

    int fd = sys_open(argv[1]);
    if (fd < 0) handle_error(-fd);
    //app_printf(2, "%s opened -> %d\n", argv[1], fd);

    ssize_t r = sys_write(fd, buffer, TEXT_SIZE);
    if (r < 0) handle_error(-r);
    //app_printf(2, "%s writed -> %d\n", argv[1], r);

    sys_exit(0);
}
