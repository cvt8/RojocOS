#include "process.h"
#include "lib.h"

#define LINE_LENGTH 80


int exec_simple_cmd(char** cmd_line) {
    int r;

    assert(cmd_line[0] != NULL);
    char* cmd = cmd_line[0];

    if (strcmp(cmd, "cd") == 0) {
        if (cmd_line[1] == NULL)
            return 0;
        
        char* path = cmd_line[1];

        r = sys_chdir(path);
        if (r < 0) {
            app_print_error(-r);
            return -r;
        }
        
        return 0;
    }
    
    if (strcmp(cmd, "pwd") == 0) {
        char buffer[256];
        r = sys_getcwd(buffer, sizeof(buffer)); // TODO: sizeof(cwd) of sizeof(cwd)-1
        assert(r == 0);

        app_printf(0, "%s\n", buffer);
        return 0;
    }

    if (strcmp(cmd, "exit") == 0) {
        sys_exit(0);
    }

    int pid = sys_fork();
    assert(pid >= 0);

    if (pid == 0) {
        // child process
        //app_printf(1, "cmdline[0] : %p\n", cmd_line[0]);
        //app_printf(1, "cmdline : %p\n", cmd_line);
        sys_execv(cmd, cmd_line);
        app_printf(1, "command not found\n");
        sys_exit(127);
    }

    // parent process

    int exit_code;
    r = sys_wait(pid, &exit_code);
    assert(r >= 0);
    r = sys_forget(pid);
    assert(r == 0);

    return exit_code;
}

void __attribute((noreturn)) shell(void) {
    while (1) {
        char buffer[256];
        int r = sys_getcwd(buffer, sizeof(buffer)); // TODO: sizeof(cwd) of sizeof(cwd)-1

        assert(r == 0);


        app_printf(3, "%s", buffer);
        app_printf(0, "$ ");

        char line[LINE_LENGTH];
        scan_line(line, LINE_LENGTH);
        char** cmd_line = split_string(line, ' ');

        //for (char** addr = cmd_line; *addr; addr++)
        //    app_printf(2, "%s\n", *addr);        

        if (line[0] != '\0') {
            exec_simple_cmd(cmd_line);
        }
    }
}

void process_main(void) {

    /*pid_t p = sys_getpid();
    srand(p);

    assert(p == 5);

    app_printf(3, "Hello, from process %d\n", p);
    app_printf(3, "Shell\n");*/


    shell();

    assert(0);
}