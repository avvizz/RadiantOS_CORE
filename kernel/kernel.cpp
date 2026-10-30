// Ядро RadiantOS на C++ (без исключений, RTTI и стандартной библиотеки)
#include "vga.h"
#include "fb.h"
#include "gui.h"
#include "interrupts.h"
#include "pmm.h"
#include "kmalloc.h"
#include "task.h"
#include "fs.h"
#include "elf.h"
#include "gdt.h"
#include "vmm.h"
#include "proc.h"
#include "initrd.h"
#include "syscall.h"
#include "io.h"

class Console {
public:
    void clear() { vga_clear(); }
    void print(const char *s) { vga_puts(s); }
    void printHex(unsigned v) { vga_puthex(v); }
    void printHex64(unsigned long long v) { vga_puthex64(v); }
    void printDec(unsigned long long v) {
        char buf[24];
        int i = 22;
        buf[23] = 0;
        if (v == 0) { print("0"); return; }
        while (v && i >= 0) {
            buf[i--] = '0' + (v % 10);
            v /= 10;
        }
        print(&buf[i + 1]);
    }
};

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}
static void serial_putc(char c) {
    while (!(inb(0x3F8 + 5) & 0x20));
    outb(0x3F8, c);
}
static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
static void serial_putdec(unsigned long long v) {
    char buf[24];
    int i = 22;
    buf[23] = 0;
    if (v == 0) { serial_puts("0"); return; }
    while (v && i >= 0) {
        buf[i--] = '0' + (v % 10);
        v /= 10;
    }
    serial_puts(&buf[i + 1]);
}

/* ---------- демо-задачи ---------- */

static void worker_a(void *arg) {
    (void)arg;
    for (int i = 0; i < 5; i++) {
        serial_puts("[task A] tick ");
        serial_putdec((unsigned)i);
        serial_puts("\n");
        /* немного «поработать» и отдать CPU */
        for (volatile int j = 0; j < 500000; j++)
            task_maybe_yield();
        task_yield();
    }
    serial_puts("[task A] done\n");
    task_exit();
}

static void worker_b(void *arg) {
    (void)arg;
    for (int i = 0; i < 5; i++) {
        serial_puts("[task B] tick ");
        serial_putdec((unsigned)i);
        serial_puts("\n");
        for (volatile int j = 0; j < 500000; j++)
            task_maybe_yield();
        task_yield();
    }
    serial_puts("[task B] done\n");
    task_exit();
}

extern "C" void kernel_main(unsigned magic, unsigned mbinfo) {
    serial_init();
    serial_puts("RadiantOS starting...\n");

    gdt_init();
    idt_init();
    syscall_init();
    pic_init();
    timer_init(100);
    keyboard_init();

    pmm_init(mbinfo);
    vmm_init();
    serial_puts("PMM free pages: ");
    serial_putdec(pmm_free_pages_count());
    serial_puts("\n");

    kmalloc_init();
    serial_puts("kmalloc ready\n");

    fs_init();
    elf_install_demos();
    initrd_load(magic, mbinfo);
    proc_init();
    serial_puts("fs ready, files=");
    serial_putdec((unsigned long long)fs_count());
    serial_puts("\n");

    task_init();
    serial_puts("scheduler ready, tasks=");
    serial_putdec(task_count());
    serial_puts("\n");

    /* Создаём две демо-задачи */
    task_create("worker-A", worker_a, 0);
    task_create("worker-B", worker_b, 0);
    serial_puts("created workers, tasks=");
    serial_putdec(task_count());
    serial_puts("\n");

    /* Даём им поработать (мы — задача kernel) */
    serial_puts("--- multitasking demo ---\n");
    for (int k = 0; k < 20; k++) {
        task_yield();
        for (volatile int j = 0; j < 100000; j++)
            task_maybe_yield();
    }
    serial_puts("--- demo finished, tasks left: ");
    serial_putdec(task_count());
    serial_puts(" ---\n");

    bool gfx = (magic == 0x36D76289) && fb_init(mbinfo);
    if (gfx) {
        vga_set_enabled(0);
        keyboard_set_echo(0);
        mouse_init();
        mouse_set_bounds(fb_width(), fb_height());
        __asm__ volatile("sti");
        serial_puts("Starting GUI...\n");
        gui_run();
    }

    Console con;
    con.clear();
    vga_write_at(0, 0, "RadiantOS x86-64");
    con.print("Multitasking demo done.\n");
    con.print("Tasks: ");
    con.printDec(task_count());
    con.print("\n");
    __asm__ volatile("sti");
    for (;;) {
        task_maybe_yield();
        __asm__ volatile("hlt");
    }
}
