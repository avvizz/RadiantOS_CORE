/* Графика: линейный framebuffer 32 бита + двойная буферизация */
#include <stdint.h>
#include "fb.h"
#include "font.h"
#include "kstring.h"

#define MAXW 1280
#define MAXH 720

static uint32_t back[MAXW * MAXH];   /* буфер, в который рисуем */
static uint32_t bg[MAXW * MAXH];     /* сохранённый фон */
static uint8_t *screen;
static uint32_t pitch;
static int W, H, ready;

int fb_init(unsigned long mbi) {
    uint8_t *p = (uint8_t *)mbi + 8;
    uint8_t *end = (uint8_t *)mbi + *(uint32_t *)mbi;
    while (p < end) {
        uint32_t type = *(uint32_t *)p, size = *(uint32_t *)(p + 4);
        if (type == 0) break;
        if (type == 8) {                       /* тег framebuffer */
            uint64_t addr = *(uint64_t *)(p + 8);
            uint32_t pt = *(uint32_t *)(p + 16);
            uint32_t w = *(uint32_t *)(p + 20), h = *(uint32_t *)(p + 24);
            uint8_t bpp = p[28], ftype = p[29];
            if (ftype == 1 && bpp == 32 && w > 0 && h > 0 &&
                addr + (uint64_t)pt * h <= 0x100000000ULL) {
                screen = (uint8_t *)addr;
                pitch = pt;
                W = w > MAXW ? MAXW : (int)w;
                H = h > MAXH ? MAXH : (int)h;
                W &= ~1;
                ready = 1;
            }
        }
        p += (size + 7) & ~7u;
    }
    return ready;
}

int fb_ready(void) { return ready; }
int fb_width(void) { return W; }
int fb_height(void) { return H; }

void fb_pixel(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    back[y * W + x] = c;
}

void fb_rect(int x, int y, int w, int h, uint32_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++) {
        uint32_t *row = &back[j * W];
        for (int i = x0; i < x1; i++) row[i] = c;
    }
}

void fb_text(int x, int y, const char *s, uint32_t color, int scale) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 32 || c > 126) c = '?';
        const unsigned char *g = font8x8[c - 32];
        for (int r = 0; r < 8; r++)
            for (int col = 0; col < 8; col++)
                if (g[r] & (1 << col))
                    fb_rect(x + col * scale, y + r * scale, scale, scale, color);
        x += 8 * scale;
    }
}

int fb_text_width(const char *s, int scale) { return (int)strlen(s) * 8 * scale; }

void fb_snapshot(void) { memcpy(bg, back, (size_t)W * H * 4); }
void fb_restore(void)  { memcpy(back, bg, (size_t)W * H * 4); }

void fb_present(void) {
    for (int y = 0; y < H; y++)
        memcpy(screen + (uint64_t)y * pitch, &back[y * W], (size_t)W * 4);
}

void fb_present_rect(int x, int y, int w, int h) {
    if (!ready || w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++)
        memcpy(screen + (uint64_t)j * pitch + (size_t)x0 * 4,
               &back[j * W + x0], (size_t)(x1 - x0) * 4);
}

/* Пиксель прямо в видеопамять (для быстрого курсора) */
void fb_screen_pixel(int x, int y, uint32_t c) {
    if (!ready || x < 0 || y < 0 || x >= W || y >= H) return;
    *(uint32_t *)(screen + (uint64_t)y * pitch + (size_t)x * 4) = c;
}

uint32_t fb_back_pixel(int x, int y) {
    if (!ready || x < 0 || y < 0 || x >= W || y >= H) return 0;
    return back[y * W + x];
}

static void hex64(char *d, uint64_t v) {
    const char *h = "0123456789ABCDEF";
    d[0] = '0'; d[1] = 'x';
    for (int i = 0; i < 16; i++) d[2 + i] = h[(v >> (60 - 4 * i)) & 0xF];
    d[18] = 0;
}

/* Красный экран при фатальной ошибке ядра */
void fb_panic(const char *name, uint64_t rip, uint64_t err) {
    if (!ready) return;
    char buf[40];
    fb_rect(0, 0, W, H, 0x8B0000);
    fb_text(20, 20, "RadiantOS KERNEL PANIC", 0xFFFFFF, 3);
    fb_text(20, 90, name, 0xFFFFFF, 2);
    fb_text(20, 130, "RIP:", 0xFFD0D0, 2);
    hex64(buf, rip); fb_text(100, 130, buf, 0xFFFFFF, 2);
    fb_text(20, 160, "ERR:", 0xFFD0D0, 2);
    hex64(buf, err); fb_text(100, 160, buf, 0xFFFFFF, 2);
    fb_text(20, 220, "System halted.", 0xFFD0D0, 2);
    fb_present();
}
