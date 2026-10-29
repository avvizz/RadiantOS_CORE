// Ядро RadiantOS на C++ (без исключений, RTTI и стандартной библиотеки)
#include "vga.h"
#include "fb.h"
#include "gui.h"
#include "interrupts.h"
#include "pmm.h"
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

/* Простой вывод в COM1 (QEMU: -serial stdio) */
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

extern "C" void kernel_main(unsigned magic, unsigned mbinfo) {
    serial_init();
    serial_puts("RadiantOS starting...\n");

    idt_init();
    pic_init();
    timer_init(100);
    keyboard_init();

    /* Физический аллокатор страниц */
    pmm_init(mbinfo);

    serial_puts("PMM: total=");
    serial_putdec(pmm_total_pages());
    serial_puts(" used=");
    serial_putdec(pmm_used_pages());
    serial_puts(" free=");
    serial_putdec(pmm_free_pages_count());
    serial_puts(" pages\n");

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

    /* Запасной текстовый режим */
    Console con;
    con.clear();
    vga_write_at(0, 0, "RadiantOS x86-64");
    con.print("Hello from RadiantOS!\n");
    con.print("Kernel: C++ + C, x86-64 (Long Mode)\n");
    con.print("Multiboot magic: ");
    con.printHex(magic);
    con.print(magic == 0x36D76289 ? "  [OK]\n" : "  [ERROR]\n");

    con.print("\n--- Physical Memory Manager ---\n");
    con.print("Total pages:  ");
    con.printDec(pmm_total_pages());
    con.print("  (");
    con.printDec(pmm_total_pages() * 4);
    con.print(" KiB)\n");
    con.print("Used pages:   ");
    con.printDec(pmm_used_pages());
    con.print("\n");
    con.print("Free pages:   ");
    con.printDec(pmm_free_pages_count());
    con.print("  (");
    con.printDec(pmm_free_pages_count() * 4);
    con.print(" KiB)\n");

    __asm__ volatile("sti");
    con.print("\nInterrupts enabled. Type something:\n> ");
    serial_puts("Text mode ready.\n");
    for (;;) __asm__ volatile("hlt");
}
