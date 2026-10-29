#ifndef FB_H
#define FB_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int  fb_init(unsigned long mbinfo);   /* 1, если GRUB дал графический режим */
int  fb_ready(void);
int  fb_width(void);
int  fb_height(void);
void fb_rect(int x, int y, int w, int h, uint32_t color);
void fb_pixel(int x, int y, uint32_t color);
void fb_text(int x, int y, const char *s, uint32_t color, int scale);
int  fb_text_width(const char *s, int scale);
void fb_snapshot(void);               /* запомнить картинку как фон */
void fb_restore(void);                /* вернуть фон */
void fb_present(void);
void fb_present_rect(int x, int y, int w, int h);
void fb_screen_pixel(int x, int y, uint32_t c);
uint32_t fb_back_pixel(int x, int y);                /* показать буфер на экране */
void fb_panic(const char *name, uint64_t rip, uint64_t err);
#ifdef __cplusplus
}
#endif
#endif
