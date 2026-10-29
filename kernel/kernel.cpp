// RadiantOS kernel – C++ (no exceptions / RTTI / libc)
#include "vga.h"
#include "interrupts.h"
#include "multiboot.h"
#include "gfx.h"

// ---- string helpers ----
static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static int strncmp(const char *a, const char *b, int n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n < 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}
static int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// ---- Radiant theme colors ----
#define COL_BG          0x0B0E14
#define COL_PANEL       0x12151C
#define COL_BORDER      0x2A3A5A
#define COL_ACCENT      0x6C5CE7   // radiant purple
#define COL_ACCENT2     0x00CEC9   // cyan
#define COL_TITLEBG     0x1A1F2E
#define COL_STATUS      0x161B26
#define COL_TEXT        0xD0D4E0
#define COL_DIM         0x6B7280
#define COL_GREEN       0x2ECC71
#define COL_YELLOW      0xF1C40F
#define COL_RED         0xE74C3C
#define COL_WHITE       0xFFFFFF

// ---- graphical terminal state ----
static int term_x = 0, term_y = 0;
static int term_cols = 80, term_rows = 30;
static int term_margin_x = 12, term_margin_y = 48;
static int term_win_x = 8, term_win_y = 32;
static int term_win_w = 0, term_win_h = 0;
static color_t term_fg = COL_TEXT;
static color_t term_bg = COL_PANEL;
static int cw = 8, ch = 16;   // char metrics (filled after gfx init)

static void term_scroll(void) {
    if (!gfx_ready) return;
    int content_h = term_rows * ch;
    // simple full redraw scroll (pixel copy)
    for (int y = 0; y < content_h - ch; y++) {
        for (int x = 0; x < term_cols * cw; x++) {
            color_t p = gfx_getpixel(term_margin_x + x, term_margin_y + y + ch);
            gfx_putpixel(term_margin_x + x, term_margin_y + y, p);
        }
    }
    gfx_fill_rect(term_margin_x, term_margin_y + content_h - ch,
                  term_cols * cw, ch, term_bg);
    term_y = term_rows - 1;
}

static void term_putc(char c) {
    if (!gfx_ready) {
        vga_putc(c);
        return;
    }
    if (c == '\n') {
        term_x = 0;
        term_y++;
        if (term_y >= term_rows) term_scroll();
        return;
    }
    if (c == '\b') {
        if (term_x > 0) {
            term_x--;
            gfx_draw_char(term_margin_x + term_x * cw,
                          term_margin_y + term_y * ch,
                          ' ', term_fg, term_bg);
        }
        return;
    }
    if (c < 32) return;
    gfx_draw_char(term_margin_x + term_x * cw,
                  term_margin_y + term_y * ch,
                  c, term_fg, term_bg);
    term_x++;
    if (term_x >= term_cols) {
        term_x = 0;
        term_y++;
        if (term_y >= term_rows) term_scroll();
    }
}

static void term_puts(const char *s) {
    while (*s) term_putc(*s++);
}

static void term_clear(void) {
    if (!gfx_ready) {
        vga_clear();
        return;
    }
    gfx_fill_rect(term_margin_x, term_margin_y,
                  term_cols * cw, term_rows * ch, term_bg);
    term_x = term_y = 0;
}

// ---- UI drawing ----
static void draw_top_bar(const char *right) {
    if (!gfx_ready) return;
    gfx_fill_rect(0, 0, fb_width, 28, COL_STATUS);
    // accent line
    gfx_hline(0, 27, fb_width, COL_ACCENT);

    gfx_draw_string(10, 6, "RadiantOS", COL_ACCENT2, COL_STATUS);
    gfx_draw_string(10 + 9*8 + 8, 6, "v0.4", COL_DIM, COL_STATUS);

    if (right) {
        int len = strlen(right);
        int x = (int)fb_width - len * cw - 12;
        if (x < 200) x = 200;
        gfx_draw_string(x, 6, right, COL_YELLOW, COL_STATUS);
    }
}

static void draw_bottom_bar(void) {
    if (!gfx_ready) return;
    int y = (int)fb_height - 24;
    gfx_fill_rect(0, y, fb_width, 24, COL_STATUS);
    gfx_hline(0, y, fb_width, COL_BORDER);
    gfx_draw_string(12, y + 5, "[Terminal]  help | clear | about | box | lines", COL_DIM, COL_STATUS);
    gfx_draw_string((int)fb_width - 12*8 - 12, y + 5, "x86-64", COL_ACCENT2, COL_STATUS);
}

static void draw_terminal_window(void) {
    if (!gfx_ready) return;
    // window chrome
    gfx_fill_rect(term_win_x, term_win_y, term_win_w, term_win_h, term_bg);
    gfx_draw_rect(term_win_x, term_win_y, term_win_w, term_win_h, COL_BORDER);
    // title bar
    gfx_fill_rect(term_win_x + 1, term_win_y + 1, term_win_w - 2, 22, COL_TITLEBG);
    gfx_draw_string(term_win_x + 10, term_win_y + 4, "Terminal", COL_WHITE, COL_TITLEBG);
    // fake window buttons
    gfx_fill_rect(term_win_x + term_win_w - 18, term_win_y + 5, 12, 12, COL_RED);
    gfx_fill_rect(term_win_x + term_win_w - 34, term_win_y + 5, 12, 12, COL_YELLOW);
    gfx_fill_rect(term_win_x + term_win_w - 50, term_win_y + 5, 12, 12, COL_GREEN);
}

static void ui_init(void) {
    if (!gfx_ready) return;
    cw = gfx_char_width();
    ch = gfx_char_height();

    // layout: top bar 28px, bottom 24px, window with margins
    term_win_x = 8;
    term_win_y = 36;
    term_win_w = (int)fb_width - 16;
    term_win_h = (int)fb_height - 36 - 32;

    term_margin_x = term_win_x + 8;
    term_margin_y = term_win_y + 28;
    term_cols = (term_win_w - 16) / cw;
    if (term_cols > 140) term_cols = 140;
    term_rows = (term_win_h - 36) / ch;
    if (term_rows < 8) term_rows = 8;

    gfx_clear(COL_BG);
    draw_top_bar("Booting...");
    draw_bottom_bar();
    draw_terminal_window();
}

static void refresh_chrome(const char *status) {
    draw_top_bar(status);
    draw_bottom_bar();
    // redraw only the window border/title (content stays)
    gfx_draw_rect(term_win_x, term_win_y, term_win_w, term_win_h, COL_BORDER);
    gfx_fill_rect(term_win_x + 1, term_win_y + 1, term_win_w - 2, 22, COL_TITLEBG);
    gfx_draw_string(term_win_x + 10, term_win_y + 4, "Terminal", COL_WHITE, COL_TITLEBG);
    gfx_fill_rect(term_win_x + term_win_w - 18, term_win_y + 5, 12, 12, COL_RED);
    gfx_fill_rect(term_win_x + term_win_w - 34, term_win_y + 5, 12, 12, COL_YELLOW);
    gfx_fill_rect(term_win_x + term_win_w - 50, term_win_y + 5, 12, 12, COL_GREEN);
}

// ---- shell ----
#define LINE_MAX 128
static char line[LINE_MAX];
static int line_len = 0;

static void prompt(void) {
    term_fg = COL_GREEN;
    term_puts("radiant");
    term_fg = COL_ACCENT2;
    term_puts("@os");
    term_fg = COL_WHITE;
    term_puts("> ");
    term_fg = COL_TEXT;
}

static void print_help(void) {
    term_puts("\n");
    term_fg = COL_ACCENT2;
    term_puts("  RadiantOS shell commands\n");
    term_fg = COL_TEXT;
    term_puts("  help          - show this help\n");
    term_puts("  clear / cls   - clear terminal\n");
    term_puts("  uptime        - system uptime\n");
    term_puts("  about         - system information\n");
    term_puts("  echo <text>   - print text\n");
    term_puts("  color <0-15>  - set text color\n");
    term_puts("  box           - demo graphical window\n");
    term_puts("  lines         - colorful lines demo\n");
    term_puts("  info          - framebuffer details\n");
    term_puts("  reboot / halt - stop the system\n\n");
}

static void cmd_about(void) {
    term_puts("\n");
    term_fg = COL_ACCENT2;
    term_puts("  RadiantOS  v0.4\n");
    term_fg = COL_TEXT;
    term_puts("  64-bit hobby operating system\n");
    term_puts("  Kernel: C++ + C, x86-64 Long Mode\n");
    term_puts("  Boot:   Multiboot2 + GRUB\n");
    term_puts("  Graphics: Linear framebuffer (RGB)\n");
    term_puts("  Features: IDT, PIC, PIT, PS/2 keyboard,\n");
    term_puts("            graphical terminal, pixel UI\n");
    term_puts("  License: MIT\n\n");
}

static void cmd_box(void) {
    if (!gfx_ready) {
        term_puts("\nGraphics not available.\n");
        return;
    }
    int wx = (int)fb_width / 2 - 180;
    int wy = (int)fb_height / 2 - 90;
    gfx_draw_window(wx, wy, 360, 180, "RadiantOS Window",
                    COL_ACCENT, COL_TITLEBG, 0x1A1A2A);
    gfx_draw_string(wx + 24, wy + 40, "Hello from the graphical UI!",
                    COL_GREEN, 0x1A1A2A);
    gfx_draw_string(wx + 24, wy + 64, "This is a simple demo window.",
                    COL_TEXT, 0x1A1A2A);
    gfx_draw_string(wx + 24, wy + 100, "Press any key to close...",
                    COL_DIM, 0x1A1A2A);
    while (!keyboard_getchar()) __asm__ volatile("hlt");
    // restore terminal area under the window
    gfx_fill_rect(wx, wy, 360, 180, term_bg);
    // redraw window chrome because we may have overwritten it
    refresh_chrome(0);
    term_puts("\n");
}

static void cmd_lines(void) {
    if (!gfx_ready) {
        term_puts("\nGraphics not available.\n");
        return;
    }
    int cx = (int)fb_width / 2;
    int cy = (int)fb_height / 2;
    color_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
        COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE, COLOR_WHITE
    };
    for (int i = 0; i < 64; i++) {
        int dx = ((i % 16) - 8) * 22;
        int dy = ((i / 16) - 2) * 42;
        gfx_line(cx, cy, cx + dx, cy + dy, colors[i % 8]);
    }
    term_puts("\nLines drawn. Type 'clear' to clean.\n");
}

static void cmd_info(void) {
    term_puts("\nFramebuffer:\n");
    if (!gfx_ready) {
        term_puts("  (text mode only)\n");
        return;
    }
    term_puts("  Address : ");
    uint64_t a = (uint64_t)(uintptr_t)fb_addr;
    char hex[] = "0123456789ABCDEF";
    term_puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        term_putc(hex[(a >> i) & 0xF]);
    term_puts("\n  Size    : ");
    char buf[16];
    int n = fb_width, i = 15;
    buf[i] = 0;
    if (!n) buf[--i] = '0';
    while (n && i) { buf[--i] = '0' + n % 10; n /= 10; }
    term_puts(&buf[i]);
    term_puts(" x ");
    n = fb_height; i = 15; buf[i] = 0;
    if (!n) buf[--i] = '0';
    while (n && i) { buf[--i] = '0' + n % 10; n /= 10; }
    term_puts(&buf[i]);
    term_puts("\n  Pitch   : ");
    n = fb_pitch; i = 15; buf[i] = 0;
    if (!n) buf[--i] = '0';
    while (n && i) { buf[--i] = '0' + n % 10; n /= 10; }
    term_puts(&buf[i]);
    term_puts("\n  BPP     : ");
    n = fb_bpp; i = 15; buf[i] = 0;
    if (!n) buf[--i] = '0';
    while (n && i) { buf[--i] = '0' + n % 10; n /= 10; }
    term_puts(&buf[i]);
    term_puts("\n\n");
}

static const color_t palette[16] = {
    COLOR_BLACK, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, COLOR_WHITE
};

static void execute(const char *cmd) {
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    if (strcmp(cmd, "help") == 0) {
        print_help();
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        term_clear();
        if (gfx_ready) refresh_chrome(0);
    } else if (strcmp(cmd, "uptime") == 0) {
        term_puts("\nUptime: ");
        uint64_t s = timer_ticks() / 100;
        char buf[16];
        int i = 15;
        buf[i] = 0;
        if (s == 0) buf[--i] = '0';
        while (s && i > 0) {
            buf[--i] = '0' + (s % 10);
            s /= 10;
        }
        term_puts(&buf[i]);
        term_puts(" seconds\n");
    } else if (strcmp(cmd, "about") == 0) {
        cmd_about();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_puts("\n");
        term_puts(cmd + 5);
        term_puts("\n");
    } else if (strncmp(cmd, "color ", 6) == 0) {
        int n = 0;
        const char *p = cmd + 6;
        while (*p >= '0' && *p <= '9') {
            n = n * 10 + (*p - '0');
            p++;
        }
        if (n > 15) n = 15;
        term_fg = palette[n];
        term_puts("\nColor set.\n");
    } else if (strcmp(cmd, "box") == 0) {
        cmd_box();
    } else if (strcmp(cmd, "lines") == 0) {
        cmd_lines();
    } else if (strcmp(cmd, "info") == 0) {
        cmd_info();
    } else if (strcmp(cmd, "reboot") == 0 || strcmp(cmd, "halt") == 0) {
        term_puts("\nSystem halted. You can close QEMU.\n");
        for (;;) __asm__ volatile("cli; hlt");
    } else {
        term_puts("\nUnknown command: ");
        term_puts(cmd);
        term_puts("\nType 'help' for list of commands.\n");
    }
}

static void shell(void) {
    prompt();
    line_len = 0;
    for (;;) {
        char c = keyboard_getchar_blocking();
        if (c == '\n' || c == '\r') {
            line[line_len] = 0;
            term_putc('\n');
            execute(line);
            line_len = 0;
            prompt();
        } else if (c == '\b' || c == 127) {
            if (line_len > 0) {
                line_len--;
                term_putc('\b');
            }
        } else if (c >= 32 && c < 127 && line_len < LINE_MAX - 1) {
            line[line_len++] = c;
            term_putc(c);
        }
    }
}

extern "C" void kernel_main(unsigned magic, unsigned long mbinfo) {
    // Try graphics first
    int have_gfx = mb2_init_framebuffer(magic, mbinfo);

    if (have_gfx) {
        ui_init();
        term_fg = COL_ACCENT2;
        term_puts("RadiantOS v0.4 – Graphical Mode\n");
        term_fg = COL_TEXT;
        term_puts("Framebuffer initialized successfully.\n");
    } else {
        // fallback pure text
        vga_clear();
        vga_status("RadiantOS v0.4", "Text mode");
        vga_puts("RadiantOS – graphics not available, using VGA text.\n");
    }

    term_puts("Kernel: C++ + C, x86-64 Long Mode\n");
    term_puts("Multiboot magic: ");
    char hex[] = "0123456789ABCDEF";
    term_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        term_putc(hex[(magic >> i) & 0xF]);
    term_puts(magic == 0x36D76289 ? "  [OK]\n" : "  [ERROR]\n");

    idt_init();
    pic_init();
    timer_init(100);
    keyboard_init();
    __asm__ volatile("sti");

    if (have_gfx)
        refresh_chrome("Ready");

    term_puts("Interrupts enabled.\n");
    term_puts("Type 'help' to see available commands.\n\n");

    shell();
}
