#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_YIELD   2
#define SYS_GETPID  3
#define SYS_OPEN    4
#define SYS_READ    5
#define SYS_CLOSE   6
#define SYS_SLEEP   7
#define SYS_TIME    8
#define SYS_SPAWN   9
#define SYS_LIST   10
#define O_CREAT  1

static inline long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory", "cc");
    return ret;
}
static inline void sys_exit(int c) { syscall3(SYS_EXIT, c, 0, 0); for(;;){} }
static inline long sys_write(int fd, const void *b, size_t n) { return syscall3(SYS_WRITE, fd, (long)b, (long)n); }
static inline void sys_yield(void) { syscall3(SYS_YIELD, 0, 0, 0); }
static inline long sys_getpid(void) { return syscall3(SYS_GETPID, 0, 0, 0); }
static inline long sys_open(const char *p, int f) { return syscall3(SYS_OPEN, (long)p, f, 0); }
static inline long sys_read(int fd, void *b, size_t n) { return syscall3(SYS_READ, fd, (long)b, (long)n); }
static inline long sys_close(int fd) { return syscall3(SYS_CLOSE, fd, 0, 0); }
static inline void sys_sleep(long t) { syscall3(SYS_SLEEP, t, 0, 0); }
static inline long sys_time(void) { return syscall3(SYS_TIME, 0, 0, 0); }
static inline long sys_spawn(const char *p) { return syscall3(SYS_SPAWN, (long)p, 0, 0); }
static inline long sys_list(char *b, size_t m) { return syscall3(SYS_LIST, (long)b, (long)m, 0); }
#endif
