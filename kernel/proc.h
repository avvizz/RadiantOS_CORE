#ifndef PROC_H
#define PROC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROC_MAX 8
#define PROC_NAME_MAX 32

enum proc_state {
    PROC_FREE = 0,
    PROC_RUNNING,
    PROC_ZOMBIE
};

struct process {
    int             pid;
    enum proc_state state;
    char            name[PROC_NAME_MAX];
    int             exit_code;
    uint64_t        task_id;
};

void proc_init(void);

/* Запустить ELF в фоне (не блокирует вызывающего). pid или -1 */
int  proc_spawn(const char *path);

/* Сколько живых процессов */
int  proc_count(void);

/* Вызвать из task после elf_run */
void proc_mark_exit(int pid, int code);

struct process *proc_get(int pid);

#ifdef __cplusplus
}
#endif

#endif
