// Ядро RadiantOS на C++ (без исключений, RTTI и стандартной библиотеки)
#include "vga.h"
#include "fb.h"
#include "gui.h"
#include "interrupts.h"
#include "pmm.h"
#include "kmalloc.h"
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
static void serial_puthex64(unsigned long long v) {
    const char *h = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        serial_putc(h[(v >> i) & 0xF]);
}

extern "C" void kernel_main(unsigned magic, unsigned mbinfo) {
    serial_init();
    serial_puts("RadiantOS starting...\n");

    idt_init();
    pic_init();
    timer_init(100);
    keyboard_init();

    pmm_init(mbinfo);
    serial_puts("PMM: free=");
    serial_putdec(pmm_free_pages_count());
    serial_puts(" pages\n");

    kmalloc_init();
    serial_puts("kmalloc ready\n");

    /* Быстрый тест кучи */
    void *a = kmalloc(64);
    void *b = kmalloc(256);
    void *c = kmalloc(1024);
    serial_puts("kmalloc(64)  -> ");
    serial_puthex64((unsigned long long)a);
    serial_puts("\nkmalloc(256) -> ");
    serial_puthex64((unsigned long long)b);
    serial_puts("\nkmalloc(1024)-> ");
    serial_puthex64((unsigned long long)c);
    serial_puts("\n");

    kfree(b);
    void *d = kmalloc(128);
    serial_puts("after free(b), kmalloc(128) -> ");
    serial_puthex64((unsigned long long)d);
    serial_puts("\nused=");
    serial_putdec(kmalloc_used_bytes());
    serial_puts(" free_heap=");
    serial_putdec(kmalloc_free_bytes());
    serial_puts("\n");

    kfree(a);
    kfree(c);
    kfree(d);

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
    con.print("Hello from RadiantOS!\n");
    con.print("PMM free pages: ");
    con.printDec(pmm_free_pages_count());
    con.print("\nkmalloc used: ");
    con.printDec(kmalloc_used_bytes());
    con.print("\n");

    __asm__ volatile("sti");
    con.print("Interrupts enabled.\n> ");
    for (;;) __asm__ volatile("hlt");
}
