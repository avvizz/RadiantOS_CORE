/* User-space программа RadiantOS (ring 3) */
#include "syscalls.h"

static size_t u_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void) {
    const char *msg = "Hello from C user program!\n";
    sys_write(1, msg, u_strlen(msg));

    const char *msg2 = "PID syscall works too.\n";
    sys_write(1, msg2, u_strlen(msg2));

    sys_exit(0);
}
