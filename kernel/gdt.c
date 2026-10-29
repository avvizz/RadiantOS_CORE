/* GDT + TSS для kernel/user сегментов и syscall (IST/RSP0) */
#include "gdt.h"
#include "kstring.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
    uint32_t base_high2;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* TSS для x86-64 */
struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[7];
static struct tss       tss;
static uint8_t          kernel_stack[16384] __attribute__((aligned(16)));

static void set_entry(int i, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t gran) {
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].base_low  = base & 0xFFFF;
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].access    = access;
    gdt[i].gran      = (limit >> 16) & 0x0F;
    gdt[i].gran     |= gran & 0xF0;
    gdt[i].base_high = (base >> 24) & 0xFF;
}

static void set_tss_entry(int i, uint64_t base, uint32_t limit) {
    struct gdt_tss_entry *e = (struct gdt_tss_entry *)&gdt[i];
    e->limit_low  = limit & 0xFFFF;
    e->base_low   = base & 0xFFFF;
    e->base_mid   = (base >> 16) & 0xFF;
    e->access     = 0x89; /* present, type=9 (TSS available) */
    e->gran       = (limit >> 16) & 0x0F;
    e->base_high  = (base >> 24) & 0xFF;
    e->base_high2 = (uint32_t)(base >> 32);
    e->reserved   = 0;
}

void gdt_init(void) {
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    /* 0: null */
    set_entry(0, 0, 0, 0, 0);

    /* 1: kernel code 0x08 — L=1 (long mode), P=1, S=1, exec/read */
    set_entry(1, 0, 0, 0x9A, 0xA0);

    /* 2: kernel data 0x10 */
    set_entry(2, 0, 0, 0x92, 0xA0);

    /* 3: user code 0x18 — DPL=3, L=1 */
    set_entry(3, 0, 0, 0xFA, 0xA0);

    /* 4: user data 0x20 — DPL=3 */
    set_entry(4, 0, 0, 0xF2, 0xA0);

    /* 5-6: TSS (16 bytes) */
    tss.rsp0 = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);
    set_tss_entry(5, (uint64_t)&tss, sizeof(tss) - 1);

    struct gdt_ptr gp = {
        .limit = sizeof(gdt) - 1,
        .base  = (uint64_t)gdt
    };

    __asm__ volatile(
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : "m"(gp)
        : "rax", "memory"
    );

    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS));
}

void tss_set_kernel_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
