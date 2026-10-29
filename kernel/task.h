#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_STACK_SIZE  (16 * 1024)   /* 16 KiB на задачу */
#define TASK_MAX         16

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
};

typedef void (*task_fn_t)(void *arg);

struct task {
    uint64_t        id;
    enum task_state state;
    uint64_t        rsp;          /* сохранённый RSP */
    void           *stack;        /* база стека (для kfree) */
    size_t          stack_size;
    task_fn_t       entry;
    void           *arg;
    const char     *name;
    struct task    *next;         /* кольцевой список ready */
};

/* Инициализация. Текущий поток становится задачей "kernel". */
void task_init(void);

/* Создать kernel-задачу. Возвращает id или (uint64_t)-1 при ошибке. */
uint64_t task_create(const char *name, task_fn_t entry, void *arg);

/* Добровольно отдать CPU. */
void task_yield(void);
void task_maybe_yield(void);  /* yield если таймер просил */

/* Завершить текущую задачу. */
void task_exit(void);

/* Принудительное переключение (вызывать из таймера и т.п.). */
void schedule(void);

/* Информация */
struct task *task_current(void);
uint64_t     task_count(void);

#ifdef __cplusplus
}
#endif

#endif
