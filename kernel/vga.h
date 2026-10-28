#ifndef VGA_H
#define VGA_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_puthex(unsigned int v);
void vga_puthex64(unsigned long long v);
void vga_write_at(int x, int y, const char *s);   /* без сдвига курсора */
void vga_write_at_color(int x, int y, const char *s, uint8_t attr);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_status(const char *left, const char *right);
void vga_draw_box(int x, int y, int w, int h, const char *title);
void vga_fill(int x, int y, int w, int h, char ch, uint8_t attr);

#ifdef __cplusplus
}
#endif
#endif
