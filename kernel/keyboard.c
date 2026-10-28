/* PS/2 клавиатура (IRQ1), scancode set 1, раскладка US — AwizzOS */
#include "interrupts.h"
#include "io.h"
#include "vga.h"

static const char map[58] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']', '\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};
static const char map_shift[58] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' '
};

#define BUF 256
static char buffer[BUF];
static volatile unsigned head = 0, tail = 0;
static int shift = 0;
static int caps = 0;

static void on_key(struct regs *r) {
    (void)r;
    uint8_t sc = inb(0x60);
    if (sc & 0x80) {                              /* отпускание */
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36) shift = 0;
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift = 1; return; } /* L/R Shift */
    if (sc == 0x3A) { caps = !caps; return; }            /* Caps Lock */
    if (sc >= 58) return;

    char c = shift ? map_shift[sc] : map[sc];
    if (!c) return;

    /* Caps влияет только на буквы */
    if (caps && c >= 'a' && c <= 'z') c -= 32;
    else if (caps && c >= 'A' && c <= 'Z') c += 32;

    buffer[head % BUF] = c;
    head++;
}

char keyboard_getchar(void) {
    if (tail == head) return 0;
    return buffer[tail++ % BUF];
}

/* блокирующее чтение с эхо (для оболочки) */
char keyboard_getchar_blocking(void) {
    char c;
    while (!(c = keyboard_getchar()))
        __asm__ volatile("hlt");
    return c;
}

void keyboard_init(void) {
    irq_install_handler(1, on_key);
    pic_unmask(1);
}
