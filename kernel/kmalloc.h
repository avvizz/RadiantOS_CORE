#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Инициализация кучи. Вызывать после pmm_init(). */
void  kmalloc_init(void);

/* Выделить size байт. Возвращает NULL при неудаче. */
void *kmalloc(size_t size);

/* Освободить блок, ранее полученный через kmalloc. */
void  kfree(void *ptr);

/* Статистика (для отладки) */
size_t kmalloc_used_bytes(void);
size_t kmalloc_free_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
