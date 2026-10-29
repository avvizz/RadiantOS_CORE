#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_YIELD  2
#define SYS_GETPID 3

static inline long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "memory", "cc"
    );
    return ret;
}

static inline void sys_exit(int code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

static inline long sys_write(int fd, const void *buf, size_t len) {
    return syscall3(SYS_WRITE, fd, (long)buf, (long)len);
}

static inline void sys_yield(void) {
    syscall3(SYS_YIELD, 0, 0, 0);
}

static inline long sys_getpid(void) {
    return syscall3(SYS_GETPID, 0, 0, 0);
}

#endif
