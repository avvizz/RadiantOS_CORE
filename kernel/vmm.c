/* Виртуальная память x86-64: PML4 / PDPT / PD / PT
   Ядро: identity 0..4GiB без User-бита.
   Процессы: своя PML4 (shared kernel entries + свои user-страницы). */

#include "vmm.h"
#include "pmm.h"
#include "kstring.h"

static addrspace_t kernel_pml4 = 0;
static addrspace_t current_as  = 0;

static inline void invlpg(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static uint64_t *alloc_table(void) {
    void *p = pmm_alloc_page();
    if (!p) return 0;
    memset(p, 0, 4096);
    return (uint64_t *)p;
}

/* phys-адрес таблицы из entry */
static uint64_t *table_ptr(uint64_t entry) {
    return (uint64_t *)(entry & PTE_ADDR);
}

int vmm_map_page(addrspace_t pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!pml4) return -1;
    phys &= PTE_ADDR;
    virt &= PTE_ADDR;

    unsigned i4 = (virt >> 39) & 0x1FF;
    unsigned i3 = (virt >> 30) & 0x1FF;
    unsigned i2 = (virt >> 21) & 0x1FF;
    unsigned i1 = (virt >> 12) & 0x1FF;

    if (!(pml4[i4] & PTE_PRESENT)) {
        uint64_t *t = alloc_table();
        if (!t) return -1;
        pml4[i4] = (uint64_t)t | PTE_PRESENT | PTE_WRITE | PTE_USER;
    }
    uint64_t *pdpt = table_ptr(pml4[i4]);

    if (!(pdpt[i3] & PTE_PRESENT)) {
        uint64_t *t = alloc_table();
        if (!t) return -1;
        pdpt[i3] = (uint64_t)t | PTE_PRESENT | PTE_WRITE | PTE_USER;
    } else if (pdpt[i3] & PTE_HUGE) {
        return -1; /* 1G page — не трогаем */
    }
    uint64_t *pd = table_ptr(pdpt[i3]);

    if (!(pd[i2] & PTE_PRESENT)) {
        uint64_t *t = alloc_table();
        if (!t) return -1;
        /* user walk: intermediate tables need U */
        uint64_t f = PTE_PRESENT | PTE_WRITE | ((flags & PTE_USER) ? PTE_USER : 0);
        pd[i2] = (uint64_t)t | f;
    } else if (pd[i2] & PTE_HUGE) {
        /* Разбиваем 2MiB huge на 512x4KiB (ядро без U, потом user-страница с U) */
        uint64_t huge_phys = pd[i2] & PTE_ADDR;
        uint64_t *pt_new = alloc_table();
        if (!pt_new) return -1;
        for (int k = 0; k < 512; k++)
            pt_new[k] = (huge_phys + (uint64_t)k * 4096) | PTE_PRESENT | PTE_WRITE;
        pd[i2] = (uint64_t)pt_new | PTE_PRESENT | PTE_WRITE |
                 ((flags & PTE_USER) ? PTE_USER : 0);
    }
    /* ensure U on intermediate if user mapping */
    if (flags & PTE_USER) {
        pml4[i4] |= PTE_USER;
        pdpt[i3] |= PTE_USER;
        pd[i2]   |= PTE_USER;
    }
    uint64_t *pt = table_ptr(pd[i2]);

    pt[i1] = phys | flags | PTE_PRESENT;
    invlpg(virt);
    return 0;
}

int vmm_map_range(addrspace_t as, uint64_t virt, uint64_t phys,
                  size_t pages, uint64_t flags) {
    for (size_t i = 0; i < pages; i++) {
        if (vmm_map_page(as, virt + i * 4096, phys + i * 4096, flags) != 0)
            return -1;
    }
    return 0;
}

/* Снять User с существующих 2MiB identity-страниц (ядро only) */
static void strip_user_from_kernel_maps(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(cr3 & PTE_ADDR);

    /* pml4[0] -> pdpt, pdpt[0..3] -> pd with 2M pages */
    if (!(pml4[0] & PTE_PRESENT)) return;
    pml4[0] &= ~PTE_USER;
    uint64_t *pdpt = table_ptr(pml4[0]);

    for (int i = 0; i < 4; i++) {
        if (!(pdpt[i] & PTE_PRESENT)) continue;
        pdpt[i] &= ~PTE_USER;
        if (pdpt[i] & PTE_HUGE) continue;
        uint64_t *pd = table_ptr(pdpt[i]);
        for (int j = 0; j < 512; j++) {
            if (pd[j] & PTE_PRESENT)
                pd[j] &= ~PTE_USER;
        }
    }
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

void vmm_init(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (addrspace_t)(cr3 & PTE_ADDR);
    current_as  = kernel_pml4;
    strip_user_from_kernel_maps();
}

addrspace_t vmm_kernel_as(void) { return kernel_pml4; }
addrspace_t vmm_current(void)   { return current_as; }

addrspace_t vmm_create_as(void) {
    uint64_t *pml4 = alloc_table();
    if (!pml4) return 0;
    /* копируем kernel PML4[0] (identity low 512GiB slot) — shared tables */
    if (kernel_pml4)
        pml4[0] = kernel_pml4[0];
    return pml4;
}

void vmm_destroy_as(addrspace_t as) {
    if (!as || as == kernel_pml4) return;
    /* не освобождаем shared kernel tables; только user PT
       упрощённо: leak user page tables (OK for hobby demos) */
    pmm_free_page(as);
}

void vmm_switch(addrspace_t as) {
    if (!as) as = kernel_pml4;
    current_as = as;
    __asm__ volatile("mov %0, %%cr3" : : "r"((uint64_t)as) : "memory");
}
