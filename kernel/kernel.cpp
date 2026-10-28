// AwizzOS kernel – C++ (no exceptions / RTTI / libc)
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

// ---- graphical terminal state ----
static int term_x = 0, term_y = 0;
static int term_cols = 80, term_rows = 40;
static int term_margin_x = 8, term_margin_y = 28;
static color_t term_fg = COLOR_LIGHTGRAY;
static color_t term_bg = 0x101018;
static color_t status_bg = 0x1A3A6E;

static void term_scroll(void) {
    int ch = gfx_char_height();
    int cw = gfx_char_width();
    int content_h = term_rows * ch;
    // simple full redraw scroll: move rows up by one char height
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
            gfx_draw_char(term_margin_x + term_x * 8,
                          term_margin_y + term_y * 16,
                          ' ', term_fg, term_bg);
        }
        return;
    }
    if (c < 32) return;
    gfx_draw_char(term_margin_x + term_x * 8,
                  term_margin_y + term_y * 16,
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
                  term_cols * 8, term_rows * 16, term_bg);
    term_x = term_y = 0;
}

static void draw_status_bar(const char *right) {
    if (!gfx_ready) return;
    gfx_fill_rect(0, 0, fb_width, 24, status_bg);
    gfx_draw_string(8, 4, "AwizzOS v0.3  |  Graphical Mode", COLOR_WHITE, status_bg);
    if (right) {
        int len = 0;
        while (right[len]) len++;
        int x = fb_width - len * 8 - 12;
        if (x < 200) x = 200;
        gfx_draw_string(x, 4, right, COLOR_YELLOW, status_bg);
    }
    // bottom bar
    gfx_fill_rect(0, fb_height - 20, fb_width, 20, status_bg);
    gfx_draw_string(8, fb_height - 16, "Type 'help' for commands", COLOR_LIGHTGRAY, status_bg);
}

static void ui_init(void) {
    if (!gfx_ready) return;
    term_cols = (fb_width - 16) / 8;
    if (term_cols > 120) term_cols = 120;
    term_rows = (fb_height - 28 - 24) / 16;
    if (term_rows < 10) term_rows = 10;
    term_margin_x = 8;
    term_margin_y = 28;

    gfx_clear(0x0A0A12);
    draw_status_bar("Booting...");
    // terminal background panel
    gfx_fill_rect(4, 26, fb_width - 8, fb_height - 50, term_bg);
    gfx_draw_rect(4, 26, fb_width - 8, fb_height - 50, 0x2A4A7E);
}

// ---- shell ----
#define LINE_MAX 128
static char line[LINE_MAX];
static int line_len = 0;

static void prompt(void) {
    term_fg = COLOR_GREEN;
    term_puts("awizz");
    term_fg = COLOR_WHITE;
    term_puts("> ");
    term_fg = COLOR_LIGHTGRAY;
}

static void print_help(void) {
    term_puts("\nAvailable commands:\n");
    term_puts("  help          - show this help\n");
    term_puts("  clear / cls   - clear screen\n");
    term_puts("  uptime        - show system uptime\n");
    term_puts("  about         - system information\n");
    term_puts("  echo <text>   - print text\n");
    term_puts("  color <n>     - set text color (0-15)\n");
    term_puts("  box           - draw a demo graphical window\n");
    term_puts("  lines         - draw colorful lines demo\n");
    term_puts("  info          - framebuffer information\n");
    term_puts("  reboot / halt - stop the system\n\n");
}

static void cmd_about(void) {
    term_puts("\n");
    term_fg = COLOR_YELLOW;
    term_puts("  AwizzOS  v0.3\n");
    term_fg = COLOR_LIGHTGRAY;
    term_puts("  64-bit hobby operating system\n");
    term_puts("  Kernel: C++ + C, x86-64 Long Mode\n");
    term_puts("  Boot:   Multiboot2 + GRUB\n");
    term_puts("  Graphics: Linear framebuffer (RGB)\n");
    term_puts("  Features: IDT, PIC, PIT, PS/2 keyboard,\n");
    term_puts("            interactive shell, pixel graphics\n");
    term_puts("  License: MIT   Author: awizz (2026)\n\n");
}

static void cmd_box(void) {
    if (!gfx_ready) {
        term_puts("\nGraphics not available.\n");
        return;
    }
    int wx = fb_width / 2 - 160;
    int wy = fb_height / 2 - 80;
    gfx_draw_window(wx, wy, 320, 160, "AwizzOS Window",
                    COLOR_CYAN, 0x1A5A9E, 0x1A1A2A);
    gfx_draw_string(wx + 24, wy + 40, "Hello from the graphical UI!",
                    COLOR_GREEN, 0x1A1A2A);
    gfx_draw_string(wx + 24, wy + 70, "Press any key to close...",
                    COLOR_LIGHTGRAY, 0x1A1A2A);
    while (!keyboard_getchar()) __asm__ volatile("hlt");
    // restore terminal area under the window
    gfx_fill_rect(wx, wy, 320, 160, term_bg);
    term_puts("\n");
}

static void cmd_lines(void) {
    if (!gfx_ready) {
        term_puts("\nGraphics not available.\n");
        return;
    }
    int cx = fb_width / 2;
    int cy = fb_height / 2;
    color_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
        COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE, COLOR_WHITE
    };
    for (int i = 0; i < 64; i++) {
        // approximate circle rays without trig
        int dx = ((i % 16) - 8) * 20;
        int dy = ((i / 16) - 2) * 40;
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
    // simple hex print
    uint64_t a = (uint64_t)(uintptr_t)fb_addr;
    char hex[] = "0123456789ABCDEF";
    term_puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        term_putc(hex[(a >> i) & 0xF]);
    term_puts("\n  Size    : ");
    // decimal width x height
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
        if (gfx_ready) draw_status_bar(0);
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

// Periodic status update from timer is still text-mode only;
// for graphics we refresh uptime on demand / every few seconds via shell.

extern "C" void kernel_main(unsigned magic, unsigned long mbinfo) {
    // Try graphics first
    int have_gfx = mb2_init_framebuffer(magic, mbinfo);

    if (have_gfx) {
        ui_init();
        term_fg = COLOR_YELLOW;
        term_puts("AwizzOS v0.3 – Graphical Mode\n");
        term_fg = COLOR_LIGHTGRAY;
        term_puts("Framebuffer initialized successfully.\n");
    } else {
        // fallback pure text
        vga_clear();
        vga_status("AwizzOS v0.3", "Text mode");
        vga_puts("AwizzOS – graphics not available, using VGA text.\n");
    }

    term_puts("Kernel: C++ + C, x86-64 Long Mode\n");
    term_puts("Multiboot magic: ");
    // print hex
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
        draw_status_bar("Ready");

    term_puts("Interrupts enabled.\n");
    term_puts("Type 'help' to see available commands.\n\n");

    shell();
}
