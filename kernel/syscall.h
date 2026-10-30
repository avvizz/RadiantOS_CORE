#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

void syscall_init(void);
uint64_t enter_user_mode(uint64_t entry, uint64_t user_stack);

#ifdef __cplusplus
}
#endif

#endif
