/* Таймер PIT (IRQ0) для AwizzOS */
#include "interrupts.h"
#include "io.h"
#include "vga.h"
#include "gfx.h"

static volatile uint64_t ticks = 0;
static uint32_t freq = 100;

static void itoa_dec(uint64_t n, char *buf, int size) {
    if (size <= 1) { buf[0] = 0; return; }
    int i = size - 1;
    buf[i] = 0;
    if (n == 0) { buf[--i] = '0'; }
    else {
        while (n && i > 0) {
            buf[--i] = '0' + (n % 10);
            n /= 10;
        }
    }
    int j = 0;
    while (buf[i]) buf[j++] = buf[i++];
    buf[j] = 0;
}

static void show_uptime(void) {
    char num[16];
    itoa_dec(ticks / freq, num, sizeof(num));
    char right[32];
    char *p = right;
    const char *prefix = "Uptime: ";
    while (*prefix) *p++ = *prefix++;
    const char *n = num;
    while (*n) *p++ = *n++;
    *p++ = 's';
    *p = 0;

    if (gfx_ready) {
        /* update only the right part of status bar */
        int len = 0;
        while (right[len]) len++;
        int x = (int)fb_width - len * 8 - 12;
        if (x < 300) x = 300;
        gfx_fill_rect(x - 4, 0, fb_width - x + 4, 24, 0x1A3A6E);
        gfx_draw_string(x, 4, right, 0xFFFF00, 0x1A3A6E);
    } else {
        vga_status("AwizzOS v0.3", right);
    }
}

static void on_tick(struct regs *r) {
    (void)r;
    ticks++;
    if (ticks % freq == 0)
        show_uptime();
}

void timer_init(uint32_t hz) {
    freq = hz ? hz : 100;
    uint32_t div = 1193182 / freq;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
    irq_install_handler(0, on_tick);
    pic_unmask(0);
}

uint64_t timer_ticks(void) { return ticks; }
