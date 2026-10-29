#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE 4096ULL

/* Инициализация по Multiboot2 info. Вызывать как можно раньше. */
void pmm_init(unsigned long mbinfo);

/* Выделить одну страницу (4 KiB). Возвращает физический адрес или NULL. */
void *pmm_alloc_page(void);

/* Освободить одну страницу. */
void  pmm_free_page(void *page);

/* Выделить n подряд идущих страниц. */
void *pmm_alloc_pages(size_t n);

/* Освободить n подряд идущих страниц. */
void  pmm_free_pages(void *page, size_t n);

/* Пометить диапазон [addr, addr+len) как занятый (ядро, FB и т.д.). */
void  pmm_reserve(uint64_t addr, uint64_t len);

/* Статистика */
size_t pmm_total_pages(void);
size_t pmm_used_pages(void);
size_t pmm_free_pages_count(void);

#ifdef __cplusplus
}
#endif

#endif
