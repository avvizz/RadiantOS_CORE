#ifndef GFX_H
#define GFX_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global framebuffer state (set by mb2_init_framebuffer) */
extern uint8_t  *fb_addr;
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern uint32_t  fb_pitch;
extern uint8_t   fb_bpp;
extern int       gfx_ready;   /* 1 if we have a usable graphics FB */

/* Colors as 0xAARRGGBB (alpha ignored for now) */
typedef uint32_t color_t;

#define COLOR_BLACK     0x000000
#define COLOR_WHITE     0xFFFFFF
#define COLOR_RED       0xFF0000
#define COLOR_GREEN     0x00FF00
#define COLOR_BLUE      0x0000FF
#define COLOR_YELLOW    0xFFFF00
#define COLOR_CYAN      0x00FFFF
#define COLOR_MAGENTA   0xFF00FF
#define COLOR_GRAY      0x808080
#define COLOR_DARKGRAY  0x404040
#define COLOR_LIGHTGRAY 0xC0C0C0
#define COLOR_ORANGE    0xFFA500
#define COLOR_NAVY      0x000080
#define COLOR_TEAL      0x008080
#define COLOR_BG        0x0D1117
#define COLOR_PANEL     0x161B22
#define COLOR_ACCENT    0x238636
#define COLOR_TITLE     0x1F6FEB

void gfx_init(uint8_t *addr, uint32_t width, uint32_t height,
              uint32_t pitch, uint8_t bpp);

void gfx_clear(color_t c);
void gfx_putpixel(int x, int y, color_t c);
color_t gfx_getpixel(int x, int y);
void gfx_fill_rect(int x, int y, int w, int h, color_t c);
void gfx_draw_rect(int x, int y, int w, int h, color_t c);
void gfx_hline(int x, int y, int len, color_t c);
void gfx_vline(int x, int y, int len, color_t c);
void gfx_line(int x0, int y0, int x1, int y1, color_t c);

/* 8x16 bitmap font */
void gfx_draw_char(int x, int y, char ch, color_t fg, color_t bg);
void gfx_draw_string(int x, int y, const char *s, color_t fg, color_t bg);
int  gfx_char_width(void);   /* 8 */
int  gfx_char_height(void);  /* 16 */

/* Higher-level UI helpers */
void gfx_draw_window(int x, int y, int w, int h, const char *title,
                     color_t border, color_t title_bg, color_t body);

#ifdef __cplusplus
}
#endif
#endif
