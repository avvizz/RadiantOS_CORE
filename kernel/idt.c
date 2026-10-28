/* IDT: таблица дескрипторов прерываний для x86-64 */
#include <stdint.h>
#include "interrupts.h"
#include "vga.h"

struct idt_entry {
    uint16_t off_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t off_mid;
    uint32_t off_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static irq_handler_t handlers[16];
extern uint64_t isr_stub_table[];

static const char *exc_names[32] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range",
    "Invalid opcode", "Device not available", "Double fault", "Coprocessor overrun",
    "Invalid TSS", "Segment not present", "Stack fault", "General protection fault",
    "Page fault", "Reserved", "x87 FPU error", "Alignment check", "Machine check",
    "SIMD exception", "Virtualization", "Control protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Hypervisor", "VMM comm", "Security",
    "Reserved"
};

static void set_gate(int n, uint64_t addr) {
    idt[n].off_low  = addr & 0xFFFF;
    idt[n].selector = 0x08;      /* кодовый сегмент из boot.asm */
    idt[n].ist      = 0;
    idt[n].flags    = 0x8E;      /* present, ring 0, interrupt gate */
    idt[n].off_mid  = (addr >> 16) & 0xFFFF;
    idt[n].off_high = (uint32_t)(addr >> 32);
    idt[n].zero     = 0;
}

void idt_init(void) {
    for (int i = 0; i < 48; i++) set_gate(i, isr_stub_table[i]);
    struct idt_ptr p = { sizeof(idt) - 1, (uint64_t)idt };
    __asm__ volatile("lidt %0" : : "m"(p));
}

void irq_install_handler(int irq, irq_handler_t fn) {
    if (irq >= 0 && irq < 16) handlers[irq] = fn;
}

/* Вызывается из isr.S для всех прерываний */
void isr_handler(struct regs *r) {
    if (r->int_no < 32) {
        vga_puts("\n*** EXCEPTION: ");
        vga_puts(exc_names[r->int_no]);
        vga_puts("\nRIP=");
        vga_puthex64(r->rip);
        vga_puts(" ERR=");
        vga_puthex64(r->err_code);
        vga_puts("\nSystem halted.\n");
        for (;;) __asm__ volatile("cli; hlt");
    }
    int irq = (int)r->int_no - 32;
    if (handlers[irq]) handlers[irq](r);
    pic_eoi(irq);
}
