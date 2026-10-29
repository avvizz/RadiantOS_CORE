// Ядро RadiantOS на C++ (без исключений, RTTI и стандартной библиотеки)
#include "vga.h"
#include "fb.h"
#include "gui.h"
#include "interrupts.h"

class Console {
public:
    void clear() { vga_clear(); }
    void print(const char *s) { vga_puts(s); }
    void printHex(unsigned v) { vga_puthex(v); }
};

extern "C" void kernel_main(unsigned magic, unsigned mbinfo) {
    idt_init();          // таблица прерываний
    pic_init();          // контроллер прерываний
    timer_init(100);     // таймер 100 Гц
    keyboard_init();     // клавиатура

    // Если GRUB включил графический режим, запускаем GUI
    bool gfx = (magic == 0x36D76289) && fb_init(mbinfo);
    if (gfx) {
        vga_set_enabled(0);
        keyboard_set_echo(0);
        mouse_init();
        mouse_set_bounds(fb_width(), fb_height());
        __asm__ volatile("sti");
        gui_run();       // сюда управление не возвращается
    }

    // Запасной вариант: текстовый режим
    Console con;
    con.clear();
    vga_write_at(0, 0, "RadiantOS x86-64");
    con.print("Hello from RadiantOS!\n");
    con.print("Kernel: C++ + C, x86-64 (Long Mode)\n");
    con.print("Graphics mode not available, text mode.\n");
    con.print("Multiboot magic: ");
    con.printHex(magic);
    con.print(magic == 0x36D76289 ? "  [OK]\n" : "  [ERROR]\n");
    __asm__ volatile("sti");
    con.print("Interrupts enabled. Type something:\n> ");
    for (;;) __asm__ volatile("hlt");
}
