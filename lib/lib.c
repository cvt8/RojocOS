#include "lib.h"
#include "x86-64.h"

#if !WEENSYOS_KERNEL          /* user‑process build only */
#include "process.h"          /* brings in inline sys_getpid()      */
#endif

// lib.c
//
//    Functions useful in both kernel and applications.


// memcpy, memmove, memset, strcmp, strlen, strnlen
//    We must provide our own implementations.

void* memcpy(void* dst, const void* src, size_t n) {
    const char* s = (const char*) src;
    for (char* d = (char*) dst; n > 0; --n, ++s, ++d) {
        *d = *s;
    }
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    const char* s = (const char*) src;
    char* d = (char*) dst;
    if (s < d && s + n > d) {
        s += n, d += n;
        while (n-- > 0) {
            *--d = *--s;
        }
    } else {
        while (n-- > 0) {
            *d++ = *s++;
        }
    }
    return dst;
}

void* memset(void* v, int c, size_t n) {
    for (char* p = (char*) v; n > 0; ++p, --n) {
        *p = c;
    }
    return v;
}

size_t strlen(const char* s) {
    size_t n;
    for (n = 0; *s != '\0'; ++s) {
        ++n;
    }
    return n;
}

size_t strnlen(const char* s, size_t maxlen) {
    size_t n;
    for (n = 0; n != maxlen && *s != '\0'; ++s) {
        ++n;
    }
    return n;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    do {
        *d++ = *src++;
    } while (d[-1]);
    return dst;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        ++a, ++b;
    }
    return ((unsigned char) *a > (unsigned char) *b)
        - ((unsigned char) *a < (unsigned char) *b);
}

char* strchr(const char* s, int c) {
    while (*s && *s != (char) c) {
        ++s;
    }
    if (*s == (char) c) {
        return (char*) s;
    } else {
        return NULL;
    }
}


// // rand, srand

// static int rand_seed_set;
// static unsigned rand_seed;

// int rand(void) {
//     if (!rand_seed_set) {
//         srand(819234718U);
//     }
//     rand_seed = rand_seed * 1664525U + 1013904223U;
//     return rand_seed & RAND_MAX;
// }

// void srand(unsigned seed) {
//     rand_seed = seed;
//     rand_seed_set = 1;
// }

// New randomness implementation
static int      rand_seed_set;
static unsigned rand_seed;

int rand(void) {
    if (!rand_seed_set) {
#if WEENSYOS_KERNEL
        extern unsigned get_entropy_value(void);
        srand(get_entropy_value());
#else           // user‑space build
       unsigned seed = sys_getrandom();     /* 128‑bit kernel entropy */
       if (!seed) {                         /* fallback if syscall fails */
           seed = (unsigned) read_cycle_counter() ^ ((unsigned) sys_getpid() << 16);
       }
       srand(seed);
#endif
    }
    rand_seed = rand_seed * 1664525U + 1013904223U;   // LCG
    return rand_seed & RAND_MAX;
}

void srand(unsigned seed) {
    rand_seed      = seed;
    rand_seed_set  = 1;
}


// console_vprintf, console_printf
//    Print a message onto the console, starting at the given cursor position.

// snprintf, vsnprintf
//    Format a string into a buffer.

static char* fill_numbuf(char* numbuf_end, unsigned long val, int base) {
    static const char upper_digits[] = "0123456789ABCDEF";
    static const char lower_digits[] = "0123456789abcdef";

    const char* digits = upper_digits;
    if (base < 0) {
        digits = lower_digits;
        base = -base;
    }

    *--numbuf_end = '\0';
    do {
        *--numbuf_end = digits[val % base];
        val /= base;
    } while (val != 0);
    return numbuf_end;
}

#define FLAG_ALT                (1<<0)
#define FLAG_ZERO               (1<<1)
#define FLAG_LEFTJUSTIFY        (1<<2)
#define FLAG_SPACEPOSITIVE      (1<<3)
#define FLAG_PLUSPOSITIVE       (1<<4)
static const char flag_chars[] = "#0- +";

#define FLAG_NUMERIC            (1<<5)
#define FLAG_SIGNED             (1<<6)
#define FLAG_NEGATIVE           (1<<7)
#define FLAG_ALT2               (1<<8)

void printer_vprintf(printer* p, int color, const char* format, va_list val) {
#define NUMBUFSIZ 24
    char numbuf[NUMBUFSIZ];

    for (; *format; ++format) {
        if (*format != '%') {
            p->putc(p, *format, color);
            continue;
        }

        // process flags
        int flags = 0;
        for (++format; *format; ++format) {
            const char* flagc = strchr(flag_chars, *format);
            if (flagc) {
                flags |= 1 << (flagc - flag_chars);
            } else {
                break;
            }
        }

        // process width
        int width = -1;
        if (*format >= '1' && *format <= '9') {
            for (width = 0; *format >= '0' && *format <= '9'; ) {
                width = 10 * width + *format++ - '0';
            }
        } else if (*format == '*') {
            width = va_arg(val, int);
            ++format;
        }

        // process precision
        int precision = -1;
        if (*format == '.') {
            ++format;
            if (*format >= '0' && *format <= '9') {
                for (precision = 0; *format >= '0' && *format <= '9'; ) {
                    precision = 10 * precision + *format++ - '0';
                }
            } else if (*format == '*') {
                precision = va_arg(val, int);
                ++format;
            }
            if (precision < 0) {
                precision = 0;
            }
        }

        // process main conversion character
        int base = 10;
        unsigned long num = 0;
        int length = 0;
        char* data = "";
    again:
        switch (*format) {
        case 'l':
        case 'z':
            length = 1;
            ++format;
            goto again;
        case 'd':
        case 'i': {
            long x = length ? va_arg(val, long) : va_arg(val, int);
            int negative = x < 0 ? FLAG_NEGATIVE : 0;
            num = negative ? -x : x;
            flags |= FLAG_NUMERIC | FLAG_SIGNED | negative;
            break;
        }
        case 'u':
        format_unsigned:
            num = length ? va_arg(val, unsigned long) : va_arg(val, unsigned);
            flags |= FLAG_NUMERIC;
            break;
        case 'x':
            base = -16;
            goto format_unsigned;
        case 'X':
            base = 16;
            goto format_unsigned;
        case 'p':
            num = (uintptr_t) va_arg(val, void*);
            base = -16;
            flags |= FLAG_ALT | FLAG_ALT2 | FLAG_NUMERIC;
            break;
        case 's':
            data = va_arg(val, char*);
            break;
        case 'C':
            color = va_arg(val, int);
            goto done;
        case 'c':
            data = numbuf;
            numbuf[0] = va_arg(val, int);
            numbuf[1] = '\0';
            break;
        default:
            data = numbuf;
            numbuf[0] = (*format ? *format : '%');
            numbuf[1] = '\0';
            if (!*format) {
                format--;
            }
            break;
        }

        if (flags & FLAG_NUMERIC) {
            data = fill_numbuf(numbuf + NUMBUFSIZ, num, base);
        }

        const char* prefix = "";
        if ((flags & FLAG_NUMERIC) && (flags & FLAG_SIGNED)) {
            if (flags & FLAG_NEGATIVE) {
                prefix = "-";
            } else if (flags & FLAG_PLUSPOSITIVE) {
                prefix = "+";
            } else if (flags & FLAG_SPACEPOSITIVE) {
                prefix = " ";
            }
        } else if ((flags & FLAG_NUMERIC) && (flags & FLAG_ALT)
                   && (base == 16 || base == -16)
                   && (num || (flags & FLAG_ALT2))) {
            prefix = (base == -16 ? "0x" : "0X");
        }

        int len;
        if (precision >= 0 && !(flags & FLAG_NUMERIC)) {
            len = strnlen(data, precision);
        } else {
            len = strlen(data);
        }
        int zeros;
        if ((flags & FLAG_NUMERIC) && precision >= 0) {
            zeros = precision > len ? precision - len : 0;
        } else if ((flags & FLAG_NUMERIC) && (flags & FLAG_ZERO)
                   && !(flags & FLAG_LEFTJUSTIFY)
                   && len + (int) strlen(prefix) < width) {
            zeros = width - len - strlen(prefix);
        } else {
            zeros = 0;
        }
        width -= len + zeros + strlen(prefix);
        for (; !(flags & FLAG_LEFTJUSTIFY) && width > 0; --width) {
            p->putc(p, ' ', color);
        }
        for (; *prefix; ++prefix) {
            p->putc(p, *prefix, color);
        }
        for (; zeros > 0; --zeros) {
            p->putc(p, '0', color);
        }
        for (; len > 0; ++data, --len) {
            p->putc(p, *data, color);
        }
        for (; width > 0; --width) {
            p->putc(p, ' ', color);
        }
    done: ;
    }
}


typedef struct console_printer {
    printer p;
    uint16_t* cursor;
} console_printer;

static void console_putc(printer* p, unsigned char c, int color) {
    console_printer* cp = (console_printer*) p;
    /*if (cp->cursor >= console + CONSOLE_ROWS * CONSOLE_COLUMNS) {
        cp->cursor = console;
    }*/
    if (c == '\n') {
        int pos = (cp->cursor - console) % CONSOLE_COLUMNS;
        for (; pos != CONSOLE_COLUMNS; pos++) {
            *cp->cursor++ = ' ' | color;
        }
    } else if (c == '\b') {
        if (cp->cursor != console) {
            *(--cp->cursor) = ' ' | color;
        }
    } else {
        *cp->cursor++ = c | color;
    }

    const int MAX_ROWS = CONSOLE_ROWS - 2;

    if (cp->cursor == console + MAX_ROWS * CONSOLE_COLUMNS) {
        // Move all characters up by one row, forgetting the first row
        for (int i = 0; i < (MAX_ROWS - 1) * CONSOLE_COLUMNS; ++i) {
            console[i] = console[i + CONSOLE_COLUMNS];
        }

        // Fill last column with space
        for (int i = (MAX_ROWS - 1) * CONSOLE_COLUMNS; i < MAX_ROWS * CONSOLE_COLUMNS; ++i) {
            console[i] = ' ' | color;
        }

        cp->cursor -= CONSOLE_COLUMNS;
    }
}

int console_vprintf(int cpos, int color, const char* format, va_list val) {
    struct console_printer cp;
    cp.p.putc = console_putc;
    if (cpos < 0 || cpos >= CONSOLE_ROWS * CONSOLE_COLUMNS) {
        cpos = 0;
    }
    cp.cursor = console + cpos;
    printer_vprintf(&cp.p, color, format, val);
    return cp.cursor - console;
}

int console_printf(int cpos, int color, const char* format, ...) {
    va_list val;
    va_start(val, format);
    cpos = console_vprintf(cpos, color, format, val);
    va_end(val);
    return cpos;
}


typedef struct string_printer {
    printer p;
    char* s;
    char* end;
} string_printer;

static void string_putc(printer* p, unsigned char c, int color) {
    string_printer* sp = (string_printer*) p;
    if (sp->s < sp->end) {
        *sp->s++ = c;
    }
    (void) color;
}

int vsnprintf(char* s, size_t size, const char* format, va_list val) {
    string_printer sp;
    sp.p.putc = string_putc;
    sp.s = s;
    if (size) {
        sp.end = s + size - 1;
        printer_vprintf(&sp.p, 0, format, val);
        *sp.s = 0;
    }
    return sp.s - s;
}

int snprintf(char* s, size_t size, const char* format, ...) {
    va_list val;
    va_start(val, format);
    int n = vsnprintf(s, size, format, val);
    va_end(val);
    return n;
}


// console_clear
//    Erases the console and moves the cursor to the upper left (CPOS(0, 0)).

void console_clear(void) {
    for (int i = 0; i < CONSOLE_ROWS * CONSOLE_COLUMNS; ++i) {
        console[i] = ' ' | 0x0700;
    }
    cursorpos = 0;
}


int string_to_char(const char *str, int *dest) {
    if (*str == '\0') return -1;

    *dest = 0;
    for (; *str; str++) {
        int digit = 0;

        switch (*str)
        {
        case '0':
            digit = 0;
            break;
        case '1':
            digit = 1;
            break;
        case '2':
            digit = 2;
            break;
        case '3':
            digit = 3;
            break;
        case '4':
            digit = 4;
            break;
        case '5':
            digit = 5;
            break;
        case '6':
            digit = 6;
            break;
        case '7':
            digit = 7;
            break;
        case '8':
            digit = 8;
            break;
        case '9':
            digit = 9;
            break;
        default:
            return -1;
        }

        *dest = *dest * 10 + digit;
    }

    return 0;
}

int atoi(const char *str) {
    if (*str == '\0') return -1;

    int n = 0;
    for (; *str; str++) {
        int digit = 0;

        switch (*str)
        {
        case '0':
            digit = 0;
            break;
        case '1':
            digit = 1;
            break;
        case '2':
            digit = 2;
            break;
        case '3':
            digit = 3;
            break;
        case '4':
            digit = 4;
            break;
        case '5':
            digit = 5;
            break;
        case '6':
            digit = 6;
            break;
        case '7':
            digit = 7;
            break;
        case '8':
            digit = 8;
            break;
        case '9':
            digit = 9;
            break;
        default:
            return -1;
        }

        n = n * 10 + digit;
    }

    return 0;
}


// Helper to add a component
#define ADD_COMP(comp, len) do { \
    if ((len) > 0) { \
        components[ncomp++] = (comp); \
    } \
} while (0)

// Helper to copy a component to dst
void copy_comp(const char *start, const char *end, char **pdst) {
    while (start < end) {
        *(*pdst)++ = *start++;
    }
}


void join_path(const char *abs_path, const char *path, char *dst) {
    // If path is absolute, just copy it to dst (removing duplicate slashes, and trailing slash unless root)
    if (path[0] == '/') {
        const char *src = path;
        char *d = dst;
        // Always start with a single slash
        *d++ = '/';
        src++;
        int last_was_slash = 1;
        while (*src) {
            if (*src == '/') {
                // Skip consecutive slashes
                while (*src == '/') src++;
                if (*src == 0) break;
                *d++ = '/';
                last_was_slash = 1;
            } else {
                *d++ = *src++;
                last_was_slash = 0;
            }
        }
        // Remove trailing slash unless the path is just "/"
        if (d > dst + 1 && *(d-1) == '/') {
            d--;
        }
        *d = 0;
        return;
    }

    // Otherwise, start with abs_path
    const char *src = abs_path;
    char *d = dst;
    if (*src != '/') {
        *d++ = '/';
    }
    while (*src) {
        *d++ = *src++;
    }
    // Remove trailing slash (unless root)
    if (d > dst + 1 && *(d-1) == '/') {
        d--;
    }
    *d = 0;

    // Now, process path components
    char temp[512];
    int tlen = 0;
    // Copy dst to temp for easier manipulation
    src = dst;
    while (*src && tlen < (int)sizeof(temp)-1) {
        temp[tlen++] = *src++;
    }
    temp[tlen] = 0;

    // Now, process each component in path
    src = path;
    while (*src) {
        // Skip slashes
        while (*src == '/') src++;
        if (!*src) break;

        // Find end of component
        const char *comp_start = src;
        while (*src && *src != '/') src++;
        const char *comp_end = src;

        int comp_len = comp_end - comp_start;

        // Get component string
        if (comp_len == 1 && comp_start[0] == '.') {
            // Ignore
            continue;
        } else if (comp_len == 2 && comp_start[0] == '.' && comp_start[1] == '.') {
            // Go up one directory, unless at root
            if (tlen > 1) {
                // Remove trailing slash if any
                if (temp[tlen-1] == '/') tlen--;
                // Remove last component
                while (tlen > 1 && temp[tlen-1] != '/') tlen--;
                temp[tlen] = 0;
            }
        } else if (comp_len > 0) {
            // Add slash if not at root
            if (tlen == 0 || temp[tlen-1] != '/') {
                if (tlen < (int)sizeof(temp)-1) temp[tlen++] = '/';
            }
            // Copy component
            for (const char *p = comp_start; p < comp_end && tlen < (int)sizeof(temp)-1; ++p) {
                temp[tlen++] = *p;
            }
            temp[tlen] = 0;
        }
    }

    // If temp is empty, set to "/"
    if (tlen == 0) {
        temp[tlen++] = '/';
        temp[tlen] = 0;
    }

    // Remove trailing slash unless the path is just "/"
    if (tlen > 1 && temp[tlen-1] == '/') {
        temp[--tlen] = 0;
    }

    // Copy temp to dst
    for (int i = 0; i <= tlen; ++i) {
        dst[i] = temp[i];
    }
}
