/* PS/2 клавиатура (IRQ1), scancode set 1, раскладка US */
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

#define BUF 128
static char buffer[BUF];
static volatile unsigned head = 0, tail = 0;
static int shift = 0;
static int echo = 1;

static void on_key(struct regs *r) {
    (void)r;
    uint8_t sc = inb(0x60);
    if (sc & 0x80) {                              /* клавиша отпущена */
        sc &= 0x7F;
        if (sc == 0x2A || sc == 0x36) shift = 0;
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift = 1; return; }
    if (sc >= 58) return;
    char c = shift ? map_shift[sc] : map[sc];
    if (!c) return;
    buffer[head % BUF] = c;
    head++;
    if (echo) vga_putc(c);                        /* эхо на экран (текстовый режим) */
}

int keyboard_pending(void) { return tail != head; }
void keyboard_set_echo(int on) { echo = on; }

char keyboard_getchar(void) {
    if (tail == head) return 0;
    return buffer[tail++ % BUF];
}

void keyboard_init(void) {
    irq_install_handler(1, on_key);
    pic_unmask(1);
}
