#ifndef WEENSYOS_PROCESS_H
#define WEENSYOS_PROCESS_H
#include "lib.h"
#include "x86-64.h"
#if WEENSYOS_KERNEL
#error "process.h should not be used by kernel code."
#endif

// process_ml.h
//
//    Support code for WeensyOS processes.


// SYSTEM CALLS

int sys_chdir(char* path) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_CHDIR), "D" /* %rdi */ (path)
                  : "cc", "memory");
    return result;
}

int sys_execv(char* path, char* argv[]) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_EXECV), "D" /* %rdi */ (path), "S" /* %rsi */ (argv)
                  : "cc", "memory");
    return result;
}

// sys_getpid
//    Return current process ID.
pid_t sys_getpid(void) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_GETPID)
                  : "cc", "memory");
    return result;
}

void sys_hello(void) {
    asm volatile ("int %0" :
                  : "i" (INT_SYS_HELLO)
                  : "cc", "memory");
}

int sys_open(const char *pathname) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_OPEN), "D" (pathname)
                  : "cc", "memory");
    return result;
}

int sys_keybord(void) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_KEYBORD)
                  : "cc", "memory");
    return result;
}

int sys_kill(pid_t pid, int sig) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_KILL), "D" (pid), "S" (sig)
                  : "cc", "memory");
    return result;
}

ssize_t sys_read(int fd, void *buf, size_t count) {
    ssize_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_READ), "D" /* %rdi */ (fd), "S" /* %rsi */ (buf), "d" /* %rdx */ (count)
                  : "cc", "memory");
    return result;
}



int sys_wait(pid_t pid, int* exit_code) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_WAIT), "D" /* %rdi */ (pid), "S" /* %rsi */ (exit_code)
                  : "cc", "memory");
    return result;
}

int sys_forget(pid_t pid) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_FORGET), "D" /* %rdi */ (pid)
                  : "cc", "memory");
    return result;
}

int sys_getcwd(char* buffer, size_t size) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_GETCWD), "D" /* %rdi */ (buffer), "S" /* %rsi */ (size)
                  : "cc", "memory");
    return result;
}

// sys_yield
//    Yield control of the CPU to the kernel. The kernel will pick another
//    process to run, if possible.
void sys_yield(void) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_SCHED_YIELD)
                  : "cc", "memory");
}

// sys_page_alloc(addr)
//    Allocate a page of memory at address `addr`. `Addr` must be page-aligned
//    (i.e., a multiple of PAGESIZE == 4096). Returns 0 on success and -1
//    on failure.
int sys_page_alloc(void* addr) {
    int result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_PAGE_ALLOC), "D" /* %rdi */ (addr)
                  : "cc", "memory");
    return result;
}

// sys_fork()
//    Fork the current process. On success, return the child's process ID to
//    the parent, and return 0 to the child. On failure, return -1.
pid_t sys_fork(void) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_FORK)
                  : "cc", "memory");
    return result;
}

// sys_exit()
//    Exit this process. Does not return.
void sys_exit(int exit_code) __attribute__((noreturn));
void sys_exit(int exit_code) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_EXIT), "D" /* %rdi */ (exit_code)
                  : "cc", "memory");
 spinloop: goto spinloop;       // should never get here
}

// sys_panic(msg)
//    Panic.
pid_t __attribute__((noreturn)) sys_panic(const char* msg) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_PANIC), "D" (msg)
                  : "cc", "memory");
 loop: goto loop;
}

#endif
