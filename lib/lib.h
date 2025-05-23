#ifndef WEENSYOS_LIB_H
#define WEENSYOS_LIB_H

#include "stdint.h"
#include "stddef.h"

// lib.h
//
//    Functions, constants, and definitions useful in both the kernel
//    and applications.
//
//    Contents: (1) C library subset, (2) system call numbers, (3) console.


// C library subset

#define NULL ((void*) 0)
#define INT_MAX 0x7FFFFFFF
#define INT64_MAX 0x7FFFFFFFFFFFFFFF

typedef __builtin_va_list va_list;
#define va_start(val, last) __builtin_va_start(val, last)
#define va_arg(val, type) __builtin_va_arg(val, type)
#define va_end(val) __builtin_va_end(val)


typedef int pid_t;                    // process IDs

#ifndef __cplusplus
typedef int bool;
#define true 1
#define false 0
#endif

void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t maxlen);
char* strcpy(char* dst, const char* src);
int strcmp(const char* a, const char* b);
char* strchr(const char* s, int c);
int snprintf(char* s, size_t size, const char* format, ...);
int vsnprintf(char* s, size_t size, const char* format, va_list val);

#define RAND_MAX 0x7FFFFFFF
int rand(void);
void srand(unsigned seed);

// Return the offset of `member` relative to the beginning of a struct type
#define offsetof(type, member)  ((size_t) (&((type*) 0)->member))

// Return the number of elements in an array
#define arraysize(array)  (sizeof(array) / sizeof(array[0]))


// Assertions

// assert(x)
//    If `x == 0`, print a message and fail.
#define assert(x) \
        do { if (!(x)) assert_fail(__FILE__, __LINE__, #x); } while (0)
void assert_fail(const char* file, int line, const char* msg)
    __attribute__((noinline, noreturn));

// panic(format, ...)
//    Print the message determined by `format` and fail.
void panic(const char* format, ...) __attribute__((noinline, noreturn));


// Min, max, and rounding operations

#define MIN(_a, _b) ({                                          \
            typeof(_a) __a = (_a); typeof(_b) __b = (_b);       \
            __a <= __b ? __a : __b; })
#define MAX(_a, _b) ({                                          \
            typeof(_a) __a = (_a); typeof(_b) __b = (_b);       \
            __a >= __b ? __a : __b; })

// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n) ({                                      \
        uint64_t __a = (uint64_t) (a);                          \
        (typeof(a)) (__a - __a % (n)); })
// Round up to the nearest multiple of n
#define ROUNDUP(a, n) ({                                        \
        uint64_t __n = (uint64_t) (n);                          \
        (typeof(a)) (ROUNDDOWN((uint64_t) (a) + __n - 1, __n)); })


// System call numbers: an application calls `int NUM` to call a system call

#define INT_SYS                 48

#define SYSCALL(n) (INT_SYS + n + 1)

#define INT_SYS_READ            SYSCALL(0)
#define INT_SYS_WRITE           SYSCALL(1)
#define INT_SYS_OPEN            SYSCALL(2)
#define INT_SYS_CLOSE           SYSCALL(3)
#define INT_SYS_STAT            SYSCALL(4)
#define INT_SYS_FSTAT           SYSCALL(5)
#define INT_SYS_SCHED_YIELD     SYSCALL(11) // 24
#define INT_SYS_GETPID          SYSCALL(12) // 39
#define INT_SYS_FORK            SYSCALL(13) // 57
#define INT_SYS_EXECV           SYSCALL(14) // 59
#define INT_SYS_EXIT            SYSCALL(15) // 60
#define INT_SYS_KILL            SYSCALL(16) // 62
#define INT_SYS_GETCWD          SYSCALL(17) // 79
#define INT_SYS_CHDIR           SYSCALL(18) // 80
#define INT_SYS_MKDIR           SYSCALL(19) // 83
#define INT_SYS_GETRANDOM       SYSCALL(20)


#define INT_SYS_HELLO           SYSCALL(6)
#define INT_SYS_FORGET          SYSCALL(7)
#define INT_SYS_WAIT            SYSCALL(8)
#define INT_SYS_PANIC           SYSCALL(-1)
#define INT_SYS_KEYBORD         SYSCALL(9)
#define INT_SYS_PAGE_ALLOC      SYSCALL(10)

#define INT_SYS_LISTDIR         SYSCALL(21)
#define INT_SYS_TOUCH           SYSCALL(22)
#define INT_SYS_REMOVE          SYSCALL(23)



// Console printing

#define CPOS(row, col)  ((row) * 80 + (col))
#define CROW(cpos)      ((cpos) / 80)
#define CCOL(cpos)      ((cpos) % 80)

#define CONSOLE_COLUMNS 80
#define CONSOLE_ROWS    25
extern uint16_t console[CONSOLE_ROWS * CONSOLE_COLUMNS];

// current position of the cursor (80 * ROW + COL)
extern int cursorpos;

// console_clear
//    Erases the console and moves the cursor to the upper left (CPOS(0, 0)).
void console_clear(void);

// console_printf(cursor, color, format, ...)
//    Format and print a message to the x86 console.
//
//    The `format` argument supports some of the C printf function's escapes:
//    %d (to print an integer in decimal notation), %u (to print an unsigned
//    integer in decimal notation), %x (to print an unsigned integer in
//    hexadecimal notation), %c (to print a character), and %s (to print a
//    string). It also takes field widths and precisions like '%10s'.
//
//    The `cursor` argument is a cursor position, such as `CPOS(r, c)` for
//    row number `r` and column number `c`.
//
//    The `color` argument is the initial color used to print. 0x0700 is a
//    good choice (grey on black). The `format` escape %C changes the color
//    being printed.  It takes an integer from the parameter list.
//
//    Returns the final position of the cursor.
int console_printf(int cpos, int color, const char* format, ...)
    __attribute__((noinline));

// console_vprintf(cpos, color, format val)
//    The vprintf version of console_printf.
int console_vprintf(int cpos, int color, const char* format, va_list val)
    __attribute__((noinline));


// Generic print library

typedef struct printer printer;
struct printer {
    void (*putc)(printer* p, unsigned char c, int color);
};

void printer_vprintf(printer* p, int color, const char* format, va_list val);


int string_to_char(const char *str, int *dest);

int atoi(const char *str);

void join_path(const char *path0, const char *path1, char *dst);

#endif /* !WEENSYOS_LIB_H */
