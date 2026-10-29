/* Физический аллокатор страниц (bitmap) для RadiantOS.
   Использует Multiboot2 Memory Map (тег type=6). */

#include "pmm.h"
#include "kstring.h"

/* Лимит: поддерживаем до 4 ГиБ физической памяти (достаточно для QEMU и большинства VM).
   4 ГиБ / 4 КиБ = 1 048 576 страниц → 128 КиБ bitmap. */
#define MAX_PHYS_PAGES  (1024ULL * 1024)
#define BITMAP_BYTES    (MAX_PHYS_PAGES / 8)

extern char _kernel_start[];
extern char _kernel_end[];

static uint8_t  bitmap[BITMAP_BYTES];   /* 1 = занята, 0 = свободна */
static size_t   total_pages;
static size_t   used_pages;
static uint64_t max_phys_addr;

/* ---------- вспомогательные операции с битами ---------- */

static inline void bit_set(size_t idx) {
    bitmap[idx / 8] |=  (1u << (idx % 8));
}

static inline void bit_clear(size_t idx) {
    bitmap[idx / 8] &= ~(1u << (idx % 8));
}

static inline int bit_test(size_t idx) {
    return (bitmap[idx / 8] >> (idx % 8)) & 1;
}

/* Пометить диапазон страниц [start_page, end_page) */
static void mark_range(size_t start, size_t end, int used) {
    if (end > total_pages) end = total_pages;
    if (start >= end) return;
    for (size_t i = start; i < end; i++) {
        int was = bit_test(i);
        if (used) {
            if (!was) { bit_set(i); used_pages++; }
        } else {
            if (was)  { bit_clear(i); used_pages--; }
        }
    }
}

void pmm_reserve(uint64_t addr, uint64_t len) {
    if (len == 0) return;
    uint64_t start = addr & ~(PAGE_SIZE - 1);
    uint64_t end   = (addr + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    mark_range((size_t)(start / PAGE_SIZE),
               (size_t)(end   / PAGE_SIZE), 1);
}

/* ---------- Multiboot2 ---------- */

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;      /* 1 = available */
    uint32_t reserved;
} __attribute__((packed));

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* entries follow */
} __attribute__((packed));

struct mb2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint16_t reserved;
} __attribute__((packed));

void pmm_init(unsigned long mbinfo) {
    /* Изначально считаем всё занятым — безопаснее */
    memset(bitmap, 0xFF, sizeof(bitmap));
    total_pages = 0;
    used_pages  = 0;
    max_phys_addr = 0;

    if (!mbinfo) return;

    uint8_t *p   = (uint8_t *)mbinfo + 8;
    uint8_t *end = (uint8_t *)mbinfo + *(uint32_t *)mbinfo;

    const struct mb2_tag_mmap *mmap = 0;
    uint64_t fb_addr = 0, fb_size = 0;

    while (p < end) {
        struct mb2_tag *tag = (struct mb2_tag *)p;
        if (tag->type == 0) break;               /* end tag */

        if (tag->type == 6) {                    /* memory map */
            mmap = (const struct mb2_tag_mmap *)p;
        } else if (tag->type == 8) {             /* framebuffer */
            const struct mb2_tag_framebuffer *fb =
                (const struct mb2_tag_framebuffer *)p;
            if (fb->bpp && fb->height && fb->pitch) {
                fb_addr = fb->addr;
                fb_size = (uint64_t)fb->pitch * fb->height;
            }
        }

        p += (tag->size + 7) & ~7u;
    }

    if (!mmap) return;

    /* 1. Находим верхнюю границу доступной памяти */
    const uint8_t *entry = (const uint8_t *)(mmap + 1);
    const uint8_t *mmap_end = (const uint8_t *)mmap + mmap->size;

    while (entry + mmap->entry_size <= mmap_end) {
        const struct mb2_mmap_entry *e = (const struct mb2_mmap_entry *)entry;
        if (e->type == 1 && e->len > 0) {
            uint64_t top = e->addr + e->len;
            if (top > max_phys_addr) max_phys_addr = top;
        }
        entry += mmap->entry_size;
    }

    if (max_phys_addr == 0) return;
    if (max_phys_addr > (uint64_t)MAX_PHYS_PAGES * PAGE_SIZE)
        max_phys_addr = (uint64_t)MAX_PHYS_PAGES * PAGE_SIZE;

    total_pages = (size_t)(max_phys_addr / PAGE_SIZE);
    if (total_pages > MAX_PHYS_PAGES) total_pages = MAX_PHYS_PAGES;

    /* Всё помечено занятым → used = total */
    used_pages = total_pages;

    /* 2. Помечаем доступные регионы как свободные */
    entry = (const uint8_t *)(mmap + 1);
    while (entry + mmap->entry_size <= mmap_end) {
        const struct mb2_mmap_entry *e = (const struct mb2_mmap_entry *)entry;
        if (e->type == 1 && e->len > 0) {
            uint64_t start = (e->addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end_a = (e->addr + e->len) & ~(PAGE_SIZE - 1);
            if (end_a > start)
                mark_range((size_t)(start / PAGE_SIZE),
                           (size_t)(end_a  / PAGE_SIZE), 0);
        }
        entry += mmap->entry_size;
    }

    /* 3. Резервируем важные области */
    /* Нижний мегабайт (BIOS, IVT, и т.д.) */
    pmm_reserve(0, 0x100000);

    /* Само ядро (включая .bss со стеком и таблицами страниц) */
    pmm_reserve((uint64_t)_kernel_start,
                (uint64_t)(_kernel_end - _kernel_start));

    /* Сам bitmap (он в .bss, уже покрыт, но на всякий случай) */
    pmm_reserve((uint64_t)bitmap, sizeof(bitmap));

    /* Framebuffer, если есть */
    if (fb_size)
        pmm_reserve(fb_addr, fb_size);

    /* Multiboot info structure (на всякий случай) */
    pmm_reserve(mbinfo, 4096);
}

/* ---------- аллокация ---------- */

void *pmm_alloc_page(void) {
    for (size_t i = 0; i < total_pages; i++) {
        if (!bit_test(i)) {
            bit_set(i);
            used_pages++;
            return (void *)(i * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void *page) {
    if (!page) return;
    uint64_t addr = (uint64_t)page;
    if (addr % PAGE_SIZE) return;
    size_t idx = (size_t)(addr / PAGE_SIZE);
    if (idx >= total_pages) return;
    if (bit_test(idx)) {
        bit_clear(idx);
        used_pages--;
    }
}

void *pmm_alloc_pages(size_t n) {
    if (n == 0) return 0;
    if (n == 1) return pmm_alloc_page();

    size_t consecutive = 0;
    size_t start = 0;

    for (size_t i = 0; i < total_pages; i++) {
        if (!bit_test(i)) {
            if (consecutive == 0) start = i;
            consecutive++;
            if (consecutive == n) {
                for (size_t j = start; j < start + n; j++) {
                    bit_set(j);
                    used_pages++;
                }
                return (void *)(start * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return 0;
}

void pmm_free_pages(void *page, size_t n) {
    if (!page || n == 0) return;
    uint64_t addr = (uint64_t)page;
    if (addr % PAGE_SIZE) return;
    size_t idx = (size_t)(addr / PAGE_SIZE);
    for (size_t i = 0; i < n; i++) {
        if (idx + i >= total_pages) break;
        if (bit_test(idx + i)) {
            bit_clear(idx + i);
            used_pages--;
        }
    }
}

size_t pmm_total_pages(void)     { return total_pages; }
size_t pmm_used_pages(void)      { return used_pages; }
size_t pmm_free_pages_count(void){ return total_pages > used_pages ? total_pages - used_pages : 0; }
