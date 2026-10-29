#ifndef VGA_H
#define VGA_H
#ifdef __cplusplus
extern "C" {
#endif
void vga_set_enabled(int on);   /* выключить в графическом режиме */
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_puthex(unsigned int v);
void vga_puthex64(unsigned long long v);
void vga_write_at(int x, int y, const char *s);   /* без сдвига курсора */
#ifdef __cplusplus
}
#endif
#endif
