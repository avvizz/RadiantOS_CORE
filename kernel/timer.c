/* Таймер PIT (IRQ0) */
#include "interrupts.h"
#include "io.h"
#include "vga.h"

static volatile uint64_t ticks = 0;
static uint32_t freq = 100;

static void show_uptime(uint64_t s) {
    char buf[24];
    int i = 21;
    buf[23] = 0; buf[22] = 's';
    do { buf[i--] = '0' + (s % 10); s /= 10; } while (s);
    vga_write_at(60, 0, "Uptime:           ");
    vga_write_at(68, 0, &buf[i + 1]);
}

static void on_tick(struct regs *r) {
    (void)r;
    ticks++;
    if (ticks % freq == 0) show_uptime(ticks / freq);
}

void timer_init(uint32_t hz) {
    freq = hz;
    uint32_t div = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
    irq_install_handler(0, on_tick);
    pic_unmask(0);
}

uint64_t timer_ticks(void) { return ticks; }
