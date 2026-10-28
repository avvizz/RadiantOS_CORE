/* Драйвер текстового режима VGA (80x25) для AwizzOS.
   Строка 0 — статус-бар, строка 24 — нижняя панель, текст 1..23. */
#include "vga.h"

#define W 80
#define H 25

static volatile unsigned short *const buf = (unsigned short *)0xB8000;
static int col = 0, row = 1;
static uint8_t color = 0x0F; /* белый на чёрном */

/* Цвета: 0=black,1=blue,2=green,3=cyan,4=red,5=magenta,6=brown,7=lightgray,
          8=darkgray,9=lightblue,10=lightgreen,11=lightcyan,12=lightred,
          13=lightmagenta,14=yellow,15=white */

static void put_at(int x, int y, char c, uint8_t attr) {
    if (x >= 0 && x < W && y >= 0 && y < H)
        buf[y * W + x] = ((uint16_t)attr << 8) | (unsigned char)c;
}

static void scroll(void) {
    /* скролл только рабочей области 1..23 */
    for (int i = W; i < W * 23; i++)
        buf[i] = buf[i + W];
    for (int i = W * 23; i < W * 24; i++)
        buf[i] = (color << 8) | ' ';
    row = 23;
}

void vga_clear(void) {
    for (int i = 0; i < W * H; i++)
        buf[i] = (color << 8) | ' ';
    col = 0;
    row = 1;
    /* верхняя и нижняя панели */
    for (int x = 0; x < W; x++) {
        put_at(x, 0, ' ', 0x1F);   /* синий фон, белый текст */
        put_at(x, 24, ' ', 0x1F);
    }
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = (bg << 4) | (fg & 0x0F);
}

void vga_putc(char c) {
    if (c == '\b') {
        if (col > 0) col--;
        else if (row > 1) { row--; col = W - 1; }
        else return;
        put_at(col, row, ' ', color);
        return;
    }
    if (c == '\t') {
        col = (col + 4) & ~3;
    } else if (c == '\n') {
        col = 0;
        row++;
    } else {
        put_at(col, row, c, color);
        col++;
    }
    if (col >= W) { col = 0; row++; }
    if (row >= 24) scroll();
}

void vga_puts(const char *s) {
    while (*s) vga_putc(*s++);
}

void vga_puthex64(unsigned long long v) {
    const char *d = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        vga_putc(d[(v >> i) & 0xF]);
}

void vga_puthex(unsigned int v) {
    const char *d = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        vga_putc(d[(v >> i) & 0xF]);
}

void vga_write_at(int x, int y, const char *s) {
    vga_write_at_color(x, y, s, 0x0E); /* жёлтый */
}

void vga_write_at_color(int x, int y, const char *s, uint8_t attr) {
    while (*s && x < W) {
        put_at(x++, y, *s++, attr);
    }
}

void vga_status(const char *left, const char *right) {
    /* очищаем строку 0 */
    for (int x = 0; x < W; x++)
        put_at(x, 0, ' ', 0x1F);
    if (left) vga_write_at_color(1, 0, left, 0x1F);
    if (right) {
        int len = 0;
        while (right[len]) len++;
        int start = W - len - 1;
        if (start < 0) start = 0;
        vga_write_at_color(start, 0, right, 0x1E); /* жёлтый на синем */
    }
}

void vga_fill(int x, int y, int w, int h, char ch, uint8_t attr) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++)
            put_at(x + dx, y + dy, ch, attr);
}

void vga_draw_box(int x, int y, int w, int h, const char *title) {
    /* простая рамка из ASCII */
    const char tl = '+', tr = '+', bl = '+', br = '+';
    const char hz = '-', vt = '|';
    uint8_t attr = 0x0B; /* cyan */
    if (w < 3 || h < 3) return;
    put_at(x, y, tl, attr);
    put_at(x + w - 1, y, tr, attr);
    put_at(x, y + h - 1, bl, attr);
    put_at(x + w - 1, y + h - 1, br, attr);
    for (int i = 1; i < w - 1; i++) {
        put_at(x + i, y, hz, attr);
        put_at(x + i, y + h - 1, hz, attr);
    }
    for (int i = 1; i < h - 1; i++) {
        put_at(x, y + i, vt, attr);
        put_at(x + w - 1, y + i, vt, attr);
    }
    if (title) {
        vga_write_at_color(x + 2, y, title, 0x0E);
    }
}
