#include "process.h"
#include "lib-malloc.h"
#include "errno.h"
// app_printf
//     A version of console_printf that picks a sensible color by process ID.

void app_printf(int colorid, const char* format, ...) {
    int color;
    if (colorid < 0) {
        color = 0x0700;
    } else {
        static const uint8_t col[] = { 0x0F, 0x0C, 0x0A, 0x09, 0x0E };
        color = col[colorid % sizeof(col)] << 8;
    }

    va_list val;
    va_start(val, format);
    cursorpos = console_vprintf(cursorpos, color, format, val);
    va_end(val);

    /*if (CROW(cursorpos) >= 23) {
        cursorpos = CPOS(0, 0);
    }*/
}


// panic, assert_fail
//     Call the INT_SYS_PANIC system call so the kernel loops until Control-C.

void panic(const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buf[160];
    memcpy(buf, "PANIC: ", 7);
    int len = vsnprintf(&buf[7], sizeof(buf) - 7, format, val) + 7;
    va_end(val);
    if (len > 0 && buf[len - 1] != '\n') {
        strcpy(buf + len - (len == (int) sizeof(buf) - 1), "\n");
    }
    (void) console_printf(CPOS(23, 0), 0xC000, "%s", buf);
    sys_panic(NULL);
 spinloop: goto spinloop;       // should never get here
}

#define SIGABRT -1



int raise(int sig) {
    return sys_kill(sys_getpid(), sig);
}

void __attribute((noreturn)) abort(void) {
    raise(SIGABRT);
 spinloop: goto spinloop;       // should never get here
}


void assert_fail(const char* file, int line, const char* msg) {
    (void) console_printf(CPOS(23, 0), 0xC000,
                          "PANIC: %s:%d: assertion '%s' failed\n",
                          file, line, msg);
    abort();
 spinloop: goto spinloop;       // should never get here
}


char scan_char(void) {
    int c;

    do {
        c = sys_keybord();
        sys_yield();
    } while (c == -1);

    return c;
}

unsigned int scan_line(char* dst, unsigned int length_max) {
    unsigned int length = 0;

    while (1) {
        unsigned char c = scan_char();

        if (c == '\b') {
            if (length != 0) {
                app_printf(0, "%c", c);
                length--;
            }
        } else if (c == '\n') {
            app_printf(0, "%c", c);
            dst[length] = '\0';
            return length;
        } else if (c >= 32 && c <= 127) {
            if (length < length_max) {
                app_printf(0, "%c", c);
                dst[length++] = c;
            }
        }
    }
}


char** split_string(char* str, char sep) {
    int s = 1;
    int n = 0;

    while (str[n]) {
        if (str[n] == sep)
            s++;
        n++;
    }
    
    char** part = malloc((s+1)*sizeof(char*));
    char* buffer = malloc(n+1);

    int word_n = 0;
    part[0] = buffer;
    part[s] = NULL;
    buffer[n] = 0;

    for (int i = 0; i < n; i++) {
        if (str[i] == sep) {
            buffer[i] = 0;
            part[++word_n] = &buffer[i+1];
        } else {
            buffer[i] = str[i];
        }
    }

    return part;
}


void app_print_error(int r) {
    switch (r) {
        case ENOENT:
            app_printf(1, "Error: %s\n", "No such file or directory");
            break;
        case EIO:
            app_printf(1, "Error: %s\n", "I/O error");
            break;
        case ENOTDIR:
            app_printf(1, "Error: %s\n", "Not a directory");
            break;
        case EINVAL:
            app_printf(1, "Error: %s\n", "Invalid argument");
            break;
        case ENOSPC:
            app_printf(1, "Error: %s\n", "No space left on device");
            break;
        case EEXIST:
            app_printf(1, "Error: %s\n", "File already exists");
            break;
        case ENAMETOOLONG:
            app_printf(1, "Error: %s\n", "File name too long");
            break;
        default:
            app_printf(1, "Error %d: %s\n", r, "Unknown error");
            break;
    }
}

__attribute__((noreturn)) void handle_error(int r) {
    app_print_error(r);
    sys_exit(r);
}
