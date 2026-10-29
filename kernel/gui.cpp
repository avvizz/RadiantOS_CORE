// Графический интерфейс RadiantOS: рабочий стол, окна, панель задач, терминал.
// Написано на C++ без стандартной библиотеки.
#include <stdint.h>
#include <stddef.h>
#include "fb.h"
#include "interrupts.h"
#include "kstring.h"
#include "gui.h"

namespace {

const int TB = 28;          // высота заголовка окна
const int TASKBAR = 36;     // высота панели задач
const int NWIN = 3;

const uint32_t C_BODY     = 0xF2F2F2;
const uint32_t C_TEXT     = 0x1A1A1A;
const uint32_t C_ACTIVE   = 0x2F6FDE;
const uint32_t C_INACTIVE = 0x6B7A90;
const uint32_t C_CLOSE    = 0xD64545;
const uint32_t C_WHITE    = 0xFFFFFF;
const uint32_t C_TASK     = 0x141B24;
const uint32_t C_BTN      = 0x2A3646;
const uint32_t C_BTN_ON   = 0x3D5A80;

enum { W_TERM = 0, W_ABOUT = 1, W_SYS = 2 };

struct Window {
    int x, y, w, h;
    const char *title;
    const char *label;
    bool visible;

    bool inside(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    bool inTitle(int px, int py) const { return inside(px, py) && py < y + TB; }
    bool inClose(int px, int py) const { return px >= x + w - TB && px < x + w && py >= y && py < y + TB; }
};

Window win[NWIN] = {
    { 40,  50, 560, 340, "Terminal",       "Terminal", true },
    { 400, 140, 440, 250, "About RadiantOS", "About",    true },
    { 100, 330, 420, 170, "System",        "System",   true },
};
int order[NWIN] = { W_ABOUT, W_SYS, W_TERM };   // последний = сверху

int dragging = -1, dragDX = 0, dragDY = 0;
bool prevLeft = false;

// ---------- вспомогательные функции ----------
char *put_s(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
char *put_n(char *p, uint64_t v) {
    char t[21]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = t[--n];
    return p;
}
bool streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
bool starts(const char *s, const char *pre) { while (*pre) { if (*s++ != *pre++) return false; } return true; }
int clampi(int v, int lo, int hi) { if (hi < lo) return lo; return v < lo ? lo : (v > hi ? hi : v); }

// ---------- терминал ----------
const int TCOLS = 32, TROWS = 14;
char tl[TROWS][TCOLS + 1];
int tn = 0;
char tin[TCOLS + 1];
int til = 0;

void t_addline(const char *s) {
    if (tn == TROWS) { memmove(tl[0], tl[1], (TROWS - 1) * (TCOLS + 1)); tn--; }
    int n = 0;
    while (s[n] && n < TCOLS) { tl[tn][n] = s[n]; n++; }
    tl[tn][n] = 0;
    tn++;
}

void t_print(const char *s) {
    char line[TCOLS + 1];
    int n = 0;
    for (; *s; s++) {
        if (*s == '\n' || n == TCOLS) {
            line[n] = 0; t_addline(line); n = 0;
            if (*s == '\n') continue;
        }
        line[n++] = *s;
    }
    if (n) { line[n] = 0; t_addline(line); }
}

void t_run(const char *cmd) {
    char buf[64];
    if (!*cmd) return;
    if (streq(cmd, "help")) {
        t_print("Commands:\nhelp clear about uptime\necho <text>");
    } else if (streq(cmd, "clear")) {
        tn = 0;
    } else if (streq(cmd, "about")) {
        t_print("RadiantOS - hobby 64-bit OS\nwritten in C and C++");
    } else if (streq(cmd, "uptime")) {
        char *p = put_s(buf, "Uptime: ");
        p = put_n(p, timer_ticks() / 100);
        p = put_s(p, " s");
        *p = 0;
        t_print(buf);
    } else if (starts(cmd, "echo ")) {
        t_print(cmd + 5);
    } else {
        char *p = put_s(buf, "Unknown: ");
        for (int i = 0; cmd[i] && i < 20; i++) *p++ = cmd[i];
        *p = 0;
        t_print(buf);
    }
}

void t_key(char c) {
    if (c == '\n') {
        char line[TCOLS + 1];
        char *p = put_s(line, "> ");
        for (int i = 0; i < til; i++) *p++ = tin[i];
        *p = 0;
        t_addline(line);
        t_run(tin);
        til = 0; tin[0] = 0;
    } else if (c == '\b') {
        if (til > 0) tin[--til] = 0;
    } else if (c >= 32 && c < 127 && til < TCOLS - 3) {
        tin[til++] = c; tin[til] = 0;
    }
}

// ---------- рисование ----------
const char *const CUR[] = {
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.....XXXXX",
    "X..X..X",
    "X.X X..X",
    "XX  X..X",
    "X    X..X",
    "     X..X",
    "      X..X",
    "      X..X",
    "       XX",
    nullptr
};

void draw_cursor(int mx, int my) {
    for (int r = 0; CUR[r]; r++)
        for (int c = 0; CUR[r][c]; c++) {
            char ch = CUR[r][c];
            if (ch == 'X') fb_pixel(mx + c, my + r, 0x000000);
            else if (ch == '.') fb_pixel(mx + c, my + r, 0xFFFFFF);
        }
}

void draw_desktop() {
    int W = fb_width(), H = fb_height();
    const int r1 = 0x0F, g1 = 0x20, b1 = 0x27, r2 = 0x2C, g2 = 0x53, b2 = 0x64;
    for (int y = 0; y < H; y++) {
        int t = y * 255 / H;
        uint32_t r = (uint32_t)((r1 * (255 - t) + r2 * t) / 255);
        uint32_t g = (uint32_t)((g1 * (255 - t) + g2 * t) / 255);
        uint32_t b = (uint32_t)((b1 * (255 - t) + b2 * t) / 255);
        fb_rect(0, y, W, 1, (r << 16) | (g << 8) | b);
    }
    fb_text(W - 310, H - TASKBAR - 60, "RadiantOS", 0x3F6E88, 4);
    fb_snapshot();
}

int active_window() {
    for (int k = NWIN - 1; k >= 0; k--)
        if (win[order[k]].visible) return order[k];
    return -1;
}

void raise_window(int i) {
    int pos = 0;
    while (order[pos] != i) pos++;
    for (; pos < NWIN - 1; pos++) order[pos] = order[pos + 1];
    order[NWIN - 1] = i;
}

void draw_window(int i, bool active) {
    const Window &w = win[i];
    bool dark = (i == W_TERM);
    fb_rect(w.x + 5, w.y + 5, w.w, w.h, 0x0B1016);                 // тень
    fb_rect(w.x - 1, w.y - 1, w.w + 2, w.h + 2, 0x10151C);         // рамка
    fb_rect(w.x, w.y, w.w, TB, active ? C_ACTIVE : C_INACTIVE);    // заголовок
    fb_text(w.x + 8, w.y + 6, w.title, C_WHITE, 2);
    fb_rect(w.x + w.w - TB + 4, w.y + 4, TB - 8, TB - 8, C_CLOSE); // кнопка закрытия
    fb_text(w.x + w.w - TB + 6, w.y + 6, "x", C_WHITE, 2);
    fb_rect(w.x, w.y + TB, w.w, w.h - TB, dark ? 0x0C1117 : C_BODY);

    int bx = w.x + 8, by = w.y + TB + 8;
    char buf[64];

    if (i == W_TERM) {
        for (int r = 0; r < tn; r++) fb_text(bx, by + r * 18, tl[r], 0x7CFC9A, 2);
        char *p = put_s(buf, "> ");
        for (int k = 0; k < til; k++) *p++ = tin[k];
        *p++ = '_'; *p = 0;
        fb_text(bx, by + TROWS * 18 + 4, buf, 0xFFFFFF, 2);
    } else if (i == W_ABOUT) {
        fb_text(bx, by,       "RadiantOS 0.2",             0x2F6FDE, 3);
        fb_text(bx, by + 36,  "64-bit kernel: C and C++", C_TEXT, 2);
        fb_text(bx, by + 60,  "Framebuffer graphics",     C_TEXT, 2);
        fb_text(bx, by + 84,  "PS/2 mouse and keyboard",  C_TEXT, 2);
        fb_text(bx, by + 116, "Drag windows by the title",C_TEXT, 2);
        fb_text(bx, by + 140, "Try: help in Terminal",    C_TEXT, 2);
    } else {
        int mx, my, mb;
        mouse_get(&mx, &my, &mb);
        char *p = put_s(buf, "Screen: ");
        p = put_n(p, (uint64_t)fb_width()); p = put_s(p, "x"); p = put_n(p, (uint64_t)fb_height());
        *p = 0; fb_text(bx, by, buf, C_TEXT, 2);
        p = put_s(buf, "Uptime: "); p = put_n(p, timer_ticks() / 100); p = put_s(p, " s");
        *p = 0; fb_text(bx, by + 24, buf, C_TEXT, 2);
        p = put_s(buf, "Mouse: "); p = put_n(p, (uint64_t)mx); p = put_s(p, ", "); p = put_n(p, (uint64_t)my);
        *p = 0; fb_text(bx, by + 48, buf, C_TEXT, 2);
        p = put_s(buf, "Ticks: "); p = put_n(p, timer_ticks());
        *p = 0; fb_text(bx, by + 72, buf, C_TEXT, 2);
    }
}

void draw_taskbar() {
    int W = fb_width(), H = fb_height();
    int ty = H - TASKBAR;
    fb_rect(0, ty, W, TASKBAR, C_TASK);
    fb_rect(0, ty, W, 1, 0x2E3A4A);
    fb_rect(4, ty + 4, 160, 28, C_ACTIVE);
    fb_text(10, ty + 10, "RadiantOS", C_WHITE, 2);
    int act = active_window();
    for (int i = 0; i < NWIN; i++) {
        int bx = 180 + i * 148;
        fb_rect(bx, ty + 4, 140, 28, (win[i].visible && i == act) ? C_BTN_ON : C_BTN);
        fb_text(bx + 8, ty + 10, win[i].label, win[i].visible ? C_WHITE : 0x8090A0, 2);
    }
    char buf[32];
    char *p = put_s(buf, "up ");
    p = put_n(p, timer_ticks() / 100); p = put_s(p, "s");
    *p = 0;
    fb_text(W - fb_text_width(buf, 2) - 10, ty + 10, buf, 0xC0D0E0, 2);
}

void draw_frame(int mx, int my) {
    fb_restore();
    int act = active_window();
    for (int k = 0; k < NWIN; k++) {
        int i = order[k];
        if (win[i].visible) draw_window(i, i == act);
    }
    draw_taskbar();
    draw_cursor(mx, my);
    fb_present();
}

// ---------- мышь ----------
void handle_mouse(int mx, int my, bool left) {
    int W = fb_width(), H = fb_height();
    bool pressed = left && !prevLeft;

    if (pressed) {
        if (my >= H - TASKBAR) {
            if (mx >= 4 && mx < 164) {
                for (int i = 0; i < NWIN; i++) win[i].visible = true;
            } else {
                for (int i = 0; i < NWIN; i++) {
                    int bx = 180 + i * 148;
                    if (mx >= bx && mx < bx + 140) {
                        if (win[i].visible && i == active_window()) win[i].visible = false;
                        else { win[i].visible = true; raise_window(i); }
                    }
                }
            }
        } else {
            for (int k = NWIN - 1; k >= 0; k--) {
                int i = order[k];
                if (!win[i].visible || !win[i].inside(mx, my)) continue;
                raise_window(i);
                if (win[i].inClose(mx, my)) win[i].visible = false;
                else if (win[i].inTitle(mx, my)) {
                    dragging = i; dragDX = mx - win[i].x; dragDY = my - win[i].y;
                }
                break;
            }
        }
    }
    if (!left) dragging = -1;
    else if (dragging >= 0) {
        Window &w = win[dragging];
        w.x = clampi(mx - dragDX, 0, W - w.w);
        w.y = clampi(my - dragDY, 0, H - TASKBAR - TB);
    }
    prevLeft = left;
}

} // namespace

extern "C" void gui_run(void) {
    t_print("Welcome to RadiantOS!\nType help for commands.");
    draw_desktop();

    unsigned lastSeq = mouse_seq() - 1;
    uint64_t lastSec = ~0ULL;
    bool dirty = true;

    for (;;) {
        int mx, my, mb;
        mouse_get(&mx, &my, &mb);
        unsigned seq = mouse_seq();
        if (seq != lastSeq) { lastSeq = seq; handle_mouse(mx, my, mb & 1); dirty = true; }

        char c;
        while ((c = keyboard_getchar()) != 0) {
            if (win[W_TERM].visible) t_key(c);
            dirty = true;
        }

        uint64_t sec = timer_ticks() / 100;
        if (sec != lastSec) { lastSec = sec; dirty = true; }

        if (dirty) { draw_frame(mx, my); dirty = false; continue; }

        // спим до следующего прерывания, не теряя событий
        __asm__ volatile("cli");
        if (mouse_seq() == lastSeq && !keyboard_pending() && timer_ticks() / 100 == lastSec)
            __asm__ volatile("sti; hlt");
        else
            __asm__ volatile("sti");
    }
}
