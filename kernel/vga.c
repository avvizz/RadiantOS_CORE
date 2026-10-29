/* Драйвер текстового режима VGA (80x25), написан на C.
   Строка 0 зарезервирована под статус-бар, текст идёт со строки 1. */
#include "vga.h"

#define W 80
#define H 25
static volatile unsigned short *const buf = (unsigned short *)0xB8000;
static int col = 0, row = 1;
static const unsigned char color = 0x0F; /* белый на чёрном */
static int enabled = 1;

void vga_set_enabled(int on) { enabled = on; }

static void scroll(void) {
    for (int i = W; i < W * (H - 1); i++) buf[i] = buf[i + W];
    for (int i = W * (H - 1); i < W * H; i++) buf[i] = (color << 8) | ' ';
    row = H - 1;
}

void vga_clear(void) {
    if (!enabled) return;
    for (int i = 0; i < W * H; i++) buf[i] = (color << 8) | ' ';
    col = 0; row = 1;
}

void vga_putc(char c) {
    if (!enabled) return;
    if (c == '\b') {
        if (col > 0) col--;
        else if (row > 1) { row--; col = W - 1; }
        else return;
        buf[row * W + col] = (color << 8) | ' ';
        return;
    }
    if (c == '\t') { col = (col + 4) & ~3; }
    else if (c == '\n') { col = 0; row++; }
    else { buf[row * W + col] = (color << 8) | (unsigned char)c; col++; }
    if (col >= W) { col = 0; row++; }
    if (row >= H) scroll();
}

void vga_puts(const char *s) { while (*s) vga_putc(*s++); }

void vga_puthex64(unsigned long long v) {
    const char *d = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 60; i >= 0; i -= 4) vga_putc(d[(v >> i) & 0xF]);
}

void vga_puthex(unsigned int v) {
    const char *d = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 28; i >= 0; i -= 4) vga_putc(d[(v >> i) & 0xF]);
}

void vga_write_at(int x, int y, const char *s) {
    if (!enabled) return;
    while (*s && x < W) buf[y * W + x++] = (0x0E << 8) | (unsigned char)*s++;  /* жёлтый */
}
