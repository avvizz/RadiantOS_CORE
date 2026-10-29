/* PS/2 мышь (IRQ12) */
#include "interrupts.h"
#include "io.h"

static volatile int mx = 100, my = 100, mb = 0;
static volatile unsigned seq = 0;
static int bw = 1024, bh = 768;
static uint8_t pkt[3];
static int idx = 0;

static void wait_write(void) { int t = 100000; while (t-- && (inb(0x64) & 2)); }
static void wait_read(void)  { int t = 100000; while (t-- && !(inb(0x64) & 1)); }
static void ctl_cmd(uint8_t c) { wait_write(); outb(0x64, c); }
static void mouse_write(uint8_t b) { ctl_cmd(0xD4); wait_write(); outb(0x60, b); }
static uint8_t mouse_read(void) { wait_read(); return inb(0x60); }

static void on_mouse(struct regs *r) {
    (void)r;
    uint8_t d = inb(0x60);
    if (idx == 0 && !(d & 8)) return;        /* потеряли синхронизацию */
    pkt[idx++] = d;
    if (idx < 3) return;
    idx = 0;
    if (pkt[0] & 0xC0) return;               /* переполнение */
    int dx = pkt[1], dy = pkt[2];
    if (pkt[0] & 0x10) dx -= 256;
    if (pkt[0] & 0x20) dy -= 256;
    int x = mx + dx, y = my - dy;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= bw) x = bw - 1;
    if (y >= bh) y = bh - 1;
    mx = x; my = y; mb = pkt[0] & 7;
    seq++;
}

void mouse_init(void) {
    ctl_cmd(0xA8);                           /* включить второй порт */
    ctl_cmd(0x20); wait_read();
    uint8_t st = inb(0x60);
    st |= 2; st &= ~0x20;                    /* IRQ12 вкл., тактирование мыши вкл. */
    ctl_cmd(0x60); wait_write(); outb(0x60, st);
    mouse_write(0xF6); mouse_read();         /* параметры по умолчанию */
    mouse_write(0xF4); mouse_read();         /* начать передачу данных */
    irq_install_handler(12, on_mouse);
    pic_unmask(12);
}

void mouse_set_bounds(int w, int h) { bw = w; bh = h; mx = w / 2; my = h / 2; }
void mouse_get(int *x, int *y, int *b) { *x = mx; *y = my; *b = mb; }
unsigned mouse_seq(void) { return seq; }
