#ifndef WEENSYOS_PROCESS_H
#define WEENSYOS_PROCESS_H
#include "lib.h"
#include "x86-64.h"
#if WEENSYOS_KERNEL
#error "process.h should not be used by kernel code."
#endif

// process.h
//
//    Support code for WeensyOS processes.


// SYSTEM CALLS

static inline int sys_chdir(const char *path) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_CHDIR), "D" /* %rdi */ (path)
                  : "cc", "memory");
    return result;
}

static inline int sys_execv(char* path, char* argv[]) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_EXECV), "D" /* %rdi */ (path), "S" /* %rsi */ (argv)
                  : "cc", "memory");
    return result;
}

// sys_getpid
//    Return current process ID.
static inline pid_t sys_getpid(void) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_GETPID)
                  : "cc", "memory");
    return result;
}

static inline void sys_hello(void) {
    asm volatile ("int %0" :
                  : "i" (INT_SYS_HELLO)
                  : "cc", "memory");
}

static inline int sys_open(const char *pathname) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_OPEN), "D" (pathname)
                  : "cc", "memory");
    return result;
}

static inline int sys_keybord(void) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_KEYBORD)
                  : "cc", "memory");
    return result;
}

static inline int sys_kill(pid_t pid, int sig) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_KILL), "D" (pid), "S" (sig)
                  : "cc", "memory");
    return result;
}

static inline ssize_t sys_read(int fd, void *buf, size_t count) {
    ssize_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_READ), "D" /* %rdi */ (fd), "S" /* %rsi */ (buf), "d" /* %rdx */ (count)
                  : "cc", "memory");
    return result;
}



static inline int sys_wait(pid_t pid, int* exit_code) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_WAIT), "D" /* %rdi */ (pid), "S" /* %rsi */ (exit_code)
                  : "cc", "memory");
    return result;
}

static inline int sys_forget(pid_t pid) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_FORGET), "D" /* %rdi */ (pid)
                  : "cc", "memory");
    return result;
}

static inline int sys_getcwd(char* buffer, size_t size) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_GETCWD), "D" /* %rdi */ (buffer), "S" /* %rsi */ (size)
                  : "cc", "memory");
    return result;
}

// sys_yield
//    Yield control of the CPU to the kernel. The kernel will pick another
//    process to run, if possible.
static inline void sys_yield(void) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_SCHED_YIELD)
                  : "cc", "memory");
}

/* Random from kernel*/
static inline unsigned sys_getrandom(void) {
    uint64_t rax;
    asm volatile ("int %1" : "=a"(rax)
                  : "i"(INT_SYS_GETRANDOM)
                  : "cc", "memory");
    return (unsigned) rax;
}


// sys_page_alloc(addr)
//    Allocate a page of memory at address `addr`. `Addr` must be page-aligned
//    (i.e., a multiple of PAGESIZE == 4096). Returns 0 on success and -1
//    on failure.
static inline int sys_page_alloc(void* addr) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_PAGE_ALLOC), "D" /* %rdi */ (addr)
                  : "cc", "memory");
    return result;
}

// sys_fork()
//    Fork the current process. On success, return the child's process ID to
//    the parent, and return 0 to the child. On failure, return -1.
static inline pid_t sys_fork(void) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_FORK)
                  : "cc", "memory");
    return result;
}

// sys_exit()
//    Exit this process. Does not return.
static inline void sys_exit(int exit_code) __attribute__((noreturn));
static inline void sys_exit(int exit_code) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_EXIT), "D" /* %rdi */ (exit_code)
                  : "cc", "memory");
 spinloop: goto spinloop;       // should never get here
}

// sys_panic(msg)
//    Panic.
static inline pid_t __attribute__((noreturn)) sys_panic(const char* msg) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_PANIC), "D" (msg)
                  : "cc", "memory");
 loop: goto loop;
}


// OTHER HELPER FUNCTIONS

// app_printf(format, ...)
//    Calls console_printf() (see lib.h). The cursor position is read from
//    `cursorpos`, a shared variable defined by the kernel, and written back
//    into that variable. The initial color is based on the current process ID.
void app_printf(int colorid, const char* format, ...);

char scan_char(void);

void scan_line(char* dst, int length_max);

char** split_string(char* str, char sep);

#endif
