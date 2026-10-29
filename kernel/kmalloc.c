/* Простой heap-аллокатор (linked-list free blocks) поверх PMM.
   Блоки выровнены на 16 байт. При нехватке места запрашиваем
   новые страницы у физического аллокатора. */

#include "kmalloc.h"
#include "pmm.h"
#include "kstring.h"

#define HEAP_ALIGN      16
#define HEAP_MAGIC      0x4B4D41u /* 'KMA' */
#define HEAP_MIN_SPLIT  64

/* Заголовок каждого блока (в начале выделенной/свободной области) */
struct block {
    uint32_t magic;          /* HEAP_MAGIC */
    uint32_t size;           /* размер полезных данных (без заголовка) */
    uint8_t  free;           /* 1 = свободен */
    uint8_t  _pad[3];
    struct block *next;      /* следующий блок в общем списке */
};

/* Весь heap — односвязный список блоков по возрастанию адреса */
static struct block *heap_head = 0;
static size_t used_bytes = 0;
static size_t free_bytes = 0;

static inline size_t align_up(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

static inline struct block *block_from_ptr(void *ptr) {
    return (struct block *)((uint8_t *)ptr - sizeof(struct block));
}

static inline void *ptr_from_block(struct block *b) {
    return (uint8_t *)b + sizeof(struct block);
}

/* Добавить новый кусок памяти от PMM в конец кучи */
static int heap_grow(size_t need) {
    /* need — сколько полезных байт нужно минимум */
    size_t total = sizeof(struct block) + need;
    total = align_up(total, PAGE_SIZE);
    size_t pages = total / PAGE_SIZE;

    void *mem = pmm_alloc_pages(pages);
    if (!mem) return 0;

    struct block *b = (struct block *)mem;
    b->magic = HEAP_MAGIC;
    b->size  = (uint32_t)(pages * PAGE_SIZE - sizeof(struct block));
    b->free  = 1;
    b->next  = 0;

    free_bytes += b->size;

    /* Вставляем в конец списка */
    if (!heap_head) {
        heap_head = b;
    } else {
        struct block *cur = heap_head;
        while (cur->next) cur = cur->next;
        cur->next = b;
    }
    return 1;
}

/* Склеить соседние свободные блоки */
static void heap_coalesce(void) {
    struct block *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            /* Проверяем, что блоки идут подряд в памяти */
            uint8_t *end = (uint8_t *)ptr_from_block(cur) + cur->size;
            if (end == (uint8_t *)cur->next) {
                cur->size += sizeof(struct block) + cur->next->size;
                cur->next  = cur->next->next;
                continue; /* снова проверить с новым next */
            }
        }
        cur = cur->next;
    }
}

void kmalloc_init(void) {
    heap_head  = 0;
    used_bytes = 0;
    free_bytes = 0;
    /* Сразу берём небольшой начальный кусок, чтобы первые kmalloc были быстрыми */
    heap_grow(PAGE_SIZE * 4 - sizeof(struct block));
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;
    size = align_up(size, HEAP_ALIGN);

    for (;;) {
        struct block *cur = heap_head;
        while (cur) {
            if (cur->free && cur->size >= size) {
                /* Нашли подходящий блок */
                size_t leftover = cur->size - size;
                if (leftover >= sizeof(struct block) + HEAP_MIN_SPLIT) {
                    /* Разрезаем */
                    struct block *n = (struct block *)((uint8_t *)ptr_from_block(cur) + size);
                    n->magic = HEAP_MAGIC;
                    n->size  = (uint32_t)(leftover - sizeof(struct block));
                    n->free  = 1;
                    n->next  = cur->next;

                    cur->size = (uint32_t)size;
                    cur->next = n;

                    free_bytes -= size + sizeof(struct block);
                    free_bytes += n->size; /* n уже учтён как free */
                } else {
                    /* Отдаём целиком */
                    free_bytes -= cur->size;
                }

                cur->free = 0;
                used_bytes += cur->size;
                return ptr_from_block(cur);
            }
            cur = cur->next;
        }

        /* Не нашли — расширяем кучу */
        if (!heap_grow(size < PAGE_SIZE ? PAGE_SIZE : size))
            return 0; /* PMM закончился */
    }
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct block *b = block_from_ptr(ptr);
    if (b->magic != HEAP_MAGIC || b->free)
        return; /* битый указатель или double-free — молча игнорируем */

    b->free = 1;
    used_bytes -= b->size;
    free_bytes += b->size;

    heap_coalesce();
}

size_t kmalloc_used_bytes(void) { return used_bytes; }
size_t kmalloc_free_bytes(void) { return free_bytes; }
