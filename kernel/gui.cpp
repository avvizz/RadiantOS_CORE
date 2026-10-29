// Графический интерфейс RadiantOS: динамические окна на kmalloc.
// Написано на C++ без стандартной библиотеки.
#include <stdint.h>
#include <stddef.h>
#include "fb.h"
#include "interrupts.h"
#include "kstring.h"
#include "kmalloc.h"
#include "fs.h"
#include "elf.h"
#include "kconsole.h"
#include "pmm.h"
#include "task.h"
#include "gui.h"

namespace {

const int TB = 28;
const int TASKBAR = 36;
const int MAX_WINDOWS = 12;

const uint32_t C_BODY     = 0xF2F2F2;
const uint32_t C_TEXT     = 0x1A1A1A;
const uint32_t C_ACTIVE   = 0x2F6FDE;
const uint32_t C_INACTIVE = 0x6B7A90;
const uint32_t C_CLOSE    = 0xD64545;
const uint32_t C_WHITE    = 0xFFFFFF;
const uint32_t C_TASK     = 0x141B24;
const uint32_t C_BTN      = 0x2A3646;
const uint32_t C_BTN_ON   = 0x3D5A80;

enum WinType { WT_TERM = 0, WT_ABOUT = 1, WT_SYS = 2, WT_NOTE = 3 };

struct Window {
    int id;
    int x, y, w, h;
    char *title;       /* kmalloc */
    char *label;       /* kmalloc */
    WinType type;
    bool visible;
    bool closable;     /* можно уничтожить */

    bool inside(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    bool inTitle(int px, int py) const { return inside(px, py) && py < y + TB; }
    bool inClose(int px, int py) const { return px >= x + w - TB && px < x + w && py >= y && py < y + TB; }
};

Window *wins[MAX_WINDOWS];
int     nwin = 0;
int     order[MAX_WINDOWS];   /* order[k] = индекс в wins, последний = сверху */
int     next_id = 1;

int dragging = -1, dragDX = 0, dragDY = 0;
bool prevLeft = false;
bool start_menu = false;
int cur_ox = -1000, cur_oy = -1000; /* last cursor pos for fast path */

/* ---------- string helpers ---------- */
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

char *kstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = (char *)kmalloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ---------- terminal ---------- */
const int TCOLS = 40, TROWS = 14;
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

/* forward */
int window_create(WinType type, const char *title, const char *label,
                  int x, int y, int w, int h, bool closable);
void window_destroy(int idx);
int find_by_type(WinType t);

static void elfinfo_print(const char *line) { t_print(line); }

static void ls_print_cb(const char *name, size_t size, void * /*user*/) {
    char line[64];
    char *p = line;
    while (*name) *p++ = *name++;
    *p++ = ' ';
    char t[12]; int n = 0; size_t s = size;
    if (!s) t[n++] = '0';
    else while (s) { t[n++] = (char)('0' + s % 10); s /= 10; }
    while (n) *p++ = t[--n];
    *p++ = 'B'; *p = 0;
    t_print(line);
}

void t_run(const char *cmd) {
    char buf[80];
    if (!*cmd) return;
    if (streq(cmd, "help")) {
        t_print("help clear about uptime mem tasks");
        t_print("echo <text> | win | about");
        t_print("ls cat <f> rm <f> touch <f>");
        t_print("write <f> <text>");
        t_print("run <elf> | elfinfo <elf>");
    } else if (streq(cmd, "clear")) {
        tn = 0;
    } else if (streq(cmd, "about")) {
        int i = find_by_type(WT_ABOUT);
        if (i >= 0) {
            wins[i]->visible = true;
        } else {
            window_create(WT_ABOUT, "About RadiantOS", "About",
                          380, 120, 440, 250, true);
        }
        t_print("About window opened");
    } else if (streq(cmd, "uptime")) {
        char *p = put_s(buf, "Uptime: ");
        p = put_n(p, timer_ticks() / 100);
        p = put_s(p, " s");
        *p = 0;
        t_print(buf);
    } else if (streq(cmd, "mem")) {
        char *p = put_s(buf, "PMM free: ");
        p = put_n(p, pmm_free_pages_count());
        p = put_s(p, " pages");
        *p = 0;
        t_print(buf);
        p = put_s(buf, "Heap used: ");
        p = put_n(p, kmalloc_used_bytes());
        p = put_s(p, " B");
        *p = 0;
        t_print(buf);
        p = put_s(buf, "Windows: ");
        p = put_n(p, (uint64_t)nwin);
        *p = 0;
        t_print(buf);
    } else if (streq(cmd, "tasks")) {
        char *p = put_s(buf, "Tasks: ");
        p = put_n(p, task_count());
        *p = 0;
        t_print(buf);
    } else if (streq(cmd, "win")) {
        char title[32];
        char *p = put_s(title, "Note #");
        p = put_n(p, (uint64_t)next_id);
        *p = 0;
        int id = window_create(WT_NOTE, title, "Note",
                               60 + (nwin % 5) * 30, 40 + (nwin % 5) * 30,
                               360, 220, true);
        if (id < 0) t_print("Cannot create window");
        else t_print("New notepad created");
    } else if (streq(cmd, "ls")) {
        fs_list(ls_print_cb, 0);
        char *p = put_s(buf, "files: ");
        p = put_n(p, (uint64_t)fs_count());
        *p = 0;
        t_print(buf);
    } else if (starts(cmd, "cat ")) {
        const char *name = cmd + 4;
        char content[256];
        int n = fs_read(name, content, sizeof(content) - 1);
        if (n < 0) t_print("No such file");
        else {
            content[n] = 0;
            t_print(content);
        }
    } else if (starts(cmd, "rm ")) {
        if (fs_delete(cmd + 3) == 0) t_print("Deleted");
        else t_print("No such file");
    } else if (starts(cmd, "touch ")) {
        if (fs_create(cmd + 6) == 0) t_print("Created");
        else t_print("Failed (exists?)");
    } else if (starts(cmd, "write ")) {
        /* write name text... */
        const char *p = cmd + 6;
        char name[FS_NAME_MAX + 1];
        int ni = 0;
        while (*p && *p != ' ' && ni < FS_NAME_MAX) name[ni++] = *p++;
        name[ni] = 0;
        if (*p == ' ') p++;
        if (!name[0]) t_print("Usage: write <file> <text>");
        else if (fs_write(name, p, strlen(p)) == 0) t_print("OK");
        else t_print("Write failed");
    } else if (starts(cmd, "run ")) {
        t_print("Running...");
        int r = elf_run(cmd + 4);
        if (r >= 0) {
            char b[40];
            char *p = put_s(b, "Exit code: ");
            p = put_n(p, (uint64_t)r);
            *p = 0;
            t_print(b);
        } else if (r == -1) t_print("Load failed");
        else t_print("Bad ELF");
    } else if (starts(cmd, "elfinfo ")) {
        elf_info(cmd + 8, elfinfo_print);
    } else if (starts(cmd, "echo ")) {
        t_print(cmd + 5);
    } else {
        char *p = put_s(buf, "Unknown: ");
        for (int i = 0; cmd[i] && i < 24; i++) *p++ = cmd[i];
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

/* ---------- window management ---------- */

int find_by_type(WinType t) {
    for (int i = 0; i < nwin; i++)
        if (wins[i] && wins[i]->type == t) return i;
    return -1;
}

int window_create(WinType type, const char *title, const char *label,
                  int x, int y, int w, int h, bool closable) {
    if (nwin >= MAX_WINDOWS) return -1;

    Window *win = (Window *)kmalloc(sizeof(Window));
    if (!win) return -1;
    memset(win, 0, sizeof(Window));

    win->id       = next_id++;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->title    = kstrdup(title);
    win->label    = kstrdup(label);
    win->type     = type;
    win->visible  = true;
    win->closable = closable;

    if (!win->title || !win->label) {
        if (win->title) kfree(win->title);
        if (win->label) kfree(win->label);
        kfree(win);
        return -1;
    }

    int idx = nwin;
    wins[nwin] = win;
    order[nwin] = nwin;
    nwin++;
    return idx;
}

void window_destroy(int idx) {
    if (idx < 0 || idx >= nwin || !wins[idx]) return;
    Window *w = wins[idx];
    if (!w->closable) {
        w->visible = false;
        return;
    }
    kfree(w->title);
    kfree(w->label);
    kfree(w);
    wins[idx] = 0;

    /* уплотняем массив */
    for (int i = idx; i < nwin - 1; i++) {
        wins[i] = wins[i + 1];
    }
    nwin--;
    wins[nwin] = 0;

    /* пересобрать order: убрать idx, сдвинуть большие индексы */
    int no = 0;
    int new_order[MAX_WINDOWS];
    for (int k = 0; k < nwin + 1; k++) {
        int oi = order[k];
        if (oi == idx) continue;
        if (oi > idx) oi--;
        if (oi >= 0 && oi < nwin)
            new_order[no++] = oi;
    }
    for (int k = 0; k < nwin; k++) order[k] = new_order[k];

    if (dragging == idx) dragging = -1;
    else if (dragging > idx) dragging--;
}

void raise_window(int i) {
    if (i < 0 || i >= nwin) return;
    int pos = 0;
    while (pos < nwin && order[pos] != i) pos++;
    if (pos >= nwin) return;
    for (; pos < nwin - 1; pos++) order[pos] = order[pos + 1];
    order[nwin - 1] = i;
}

int active_window() {
    for (int k = nwin - 1; k >= 0; k--) {
        int i = order[k];
        if (i >= 0 && i < nwin && wins[i] && wins[i]->visible)
            return i;
    }
    return -1;
}


/* ---------- drawing ---------- */
const char *const CUR[] = {
    "X","XX","X.X","X..X","X...X","X....X","X.....X","X......X",
    "X.......X","X........X","X.....XXXXX","X..X..X","X.X X..X",
    "XX  X..X","X    X..X","     X..X","      X..X","      X..X","       XX",
    nullptr
};
const int CUR_W = 12, CUR_H = 20;

void draw_cursor_on_screen(int mx, int my) {
    /* стереть старый курсор из back → screen */
    if (cur_ox > -900) {
        fb_present_rect(cur_ox - 1, cur_oy - 1, CUR_W + 2, CUR_H + 2);
    }
    /* нарисовать новый прямо на экране */
    for (int r = 0; CUR[r]; r++)
        for (int col = 0; CUR[r][col]; col++) {
            char ch = CUR[r][col];
            if (ch == 'X') fb_screen_pixel(mx + col, my + r, 0x000000);
            else if (ch == '.') fb_screen_pixel(mx + col, my + r, 0xFFFFFF);
        }
    cur_ox = mx; cur_oy = my;
}

void draw_desktop() {
    int W = fb_width(), H = fb_height();
    /* более насыщенный градиент */
    for (int y = 0; y < H; y++) {
        int t = y * 255 / (H ? H : 1);
        uint32_t r = (uint32_t)((12 * (255 - t) + 40 * t) / 255);
        uint32_t g = (uint32_t)((28 * (255 - t) + 70 * t) / 255);
        uint32_t b = (uint32_t)((48 * (255 - t) + 110 * t) / 255);
        fb_rect(0, y, W, 1, (r << 16) | (g << 8) | b);
    }
    /* декоративные «иконки» на рабочем столе */
    fb_rect(24, 24, 64, 64, 0x1A2332);
    fb_rect(26, 26, 60, 40, 0x2F6FDE);
    fb_text(30, 72, "Term", 0xC0D0E0, 1);

    fb_rect(24, 110, 64, 64, 0x1A2332);
    fb_rect(26, 112, 60, 40, 0x238636);
    fb_text(28, 158, "About", 0xC0D0E0, 1);

    fb_text(W - 280, H - TASKBAR - 50, "RadiantOS", 0x4A7A9A, 3);
    fb_snapshot();
}

void draw_window(int i, bool active) {
    Window *w = wins[i];
    if (!w || !w->visible) return;

    bool dark = (w->type == WT_TERM || w->type == WT_NOTE);
    /* тень */
    fb_rect(w->x + 6, w->y + 6, w->w, w->h, 0x080C12);
    /* рамка */
    fb_rect(w->x - 1, w->y - 1, w->w + 2, w->h + 2, active ? 0x3D7AE8 : 0x2A3344);
    /* заголовок */
    fb_rect(w->x, w->y, w->w, TB, active ? C_ACTIVE : C_INACTIVE);
    fb_text(w->x + 10, w->y + 6, w->title, C_WHITE, 2);
    /* close */
    fb_rect(w->x + w->w - TB + 4, w->y + 4, TB - 8, TB - 8, C_CLOSE);
    fb_text(w->x + w->w - TB + 8, w->y + 6, "x", C_WHITE, 2);
    fb_rect(w->x, w->y + TB, w->w, w->h - TB, dark ? 0x0C1117 : C_BODY);

    int bx = w->x + 8, by = w->y + TB + 8;
    char buf[64];

    if (w->type == WT_TERM) {
        for (int r = 0; r < tn; r++) fb_text(bx, by + r * 18, tl[r], 0x7CFC9A, 2);
        char *p = put_s(buf, "> ");
        for (int k = 0; k < til; k++) *p++ = tin[k];
        *p++ = '_'; *p = 0;
        fb_text(bx, by + TROWS * 18 + 4, buf, 0xFFFFFF, 2);
    } else if (w->type == WT_ABOUT) {
        fb_text(bx, by,       "RadiantOS 0.4",             0x2F6FDE, 3);
        fb_text(bx, by + 36,  "64-bit kernel: C and C++", C_TEXT, 2);
        fb_text(bx, by + 60,  "1280x720 graphics",        C_TEXT, 2);
        fb_text(bx, by + 84,  "User mode + syscalls",     C_TEXT, 2);
        fb_text(bx, by + 108, "Dynamic windows + RAM FS", C_TEXT, 2);
        fb_text(bx, by + 140, "Try: run hello.elf",       C_TEXT, 2);
    } else if (w->type == WT_SYS) {
        int mx, my, mb;
        mouse_get(&mx, &my, &mb);
        char *p = put_s(buf, "Screen: ");
        p = put_n(p, (uint64_t)fb_width()); p = put_s(p, "x"); p = put_n(p, (uint64_t)fb_height());
        *p = 0; fb_text(bx, by, buf, C_TEXT, 2);
        p = put_s(buf, "Uptime: "); p = put_n(p, timer_ticks() / 100); p = put_s(p, " s");
        *p = 0; fb_text(bx, by + 24, buf, C_TEXT, 2);
        p = put_s(buf, "Mouse: "); p = put_n(p, (uint64_t)mx); p = put_s(p, ", "); p = put_n(p, (uint64_t)my);
        *p = 0; fb_text(bx, by + 48, buf, C_TEXT, 2);
        p = put_s(buf, "Windows: "); p = put_n(p, (uint64_t)nwin);
        *p = 0; fb_text(bx, by + 72, buf, C_TEXT, 2);
        p = put_s(buf, "Free pages: "); p = put_n(p, pmm_free_pages_count());
        *p = 0; fb_text(bx, by + 96, buf, C_TEXT, 2);
        p = put_s(buf, "Heap used: "); p = put_n(p, kmalloc_used_bytes()); p = put_s(p, " B");
        *p = 0; fb_text(bx, by + 120, buf, C_TEXT, 2);
    } else if (w->type == WT_NOTE) {
        fb_text(bx, by,      "Notepad", 0x7CFC9A, 2);
        fb_text(bx, by + 28, "Created with kmalloc.", C_TEXT, 2);
        fb_text(bx, by + 52, "Close with X to free.", C_TEXT, 2);
        char *p = put_s(buf, "id: ");
        p = put_n(p, (uint64_t)w->id);
        *p = 0;
        fb_text(bx, by + 90, buf, 0x8090A0, 2);
    }
}

void draw_start_menu() {
    if (!start_menu) return;
    int H = fb_height();
    int my = H - TASKBAR - 200;
    fb_rect(4, my, 200, 196, 0x1B2433);
    fb_rect(4, my, 200, 196, 0x1B2433);
    fb_rect(4, my, 200, 2, C_ACTIVE);
    fb_text(16, my + 12, "RadiantOS Menu", C_WHITE, 2);

    fb_rect(12, my + 40, 184, 28, C_BTN);
    fb_text(24, my + 46, "Terminal", C_WHITE, 2);

    fb_rect(12, my + 74, 184, 28, C_BTN);
    fb_text(24, my + 80, "About", C_WHITE, 2);

    fb_rect(12, my + 108, 184, 28, C_BTN);
    fb_text(24, my + 114, "System", C_WHITE, 2);

    fb_rect(12, my + 142, 184, 28, C_BTN);
    fb_text(24, my + 148, "New Note", C_WHITE, 2);
}

void draw_taskbar() {
    int W = fb_width(), H = fb_height();
    int ty = H - TASKBAR;
    fb_rect(0, ty, W, TASKBAR, 0x0F1520);
    fb_rect(0, ty, W, 2, C_ACTIVE);

    /* Start */
    fb_rect(4, ty + 4, 100, 28, start_menu ? C_BTN_ON : C_ACTIVE);
    fb_text(20, ty + 10, "Start", C_WHITE, 2);

    /* +Win */
    fb_rect(110, ty + 4, 70, 28, C_BTN);
    fb_text(122, ty + 10, "+Win", C_WHITE, 2);

    int act = active_window();
    int bx = 190;
    for (int i = 0; i < nwin && bx + 120 < W - 140; i++) {
        if (!wins[i]) continue;
        fb_rect(bx, ty + 4, 110, 28, (wins[i]->visible && i == act) ? C_BTN_ON : C_BTN);
        fb_text(bx + 6, ty + 10, wins[i]->label,
                wins[i]->visible ? C_WHITE : 0x8090A0, 2);
        bx += 116;
    }

    /* часы HH:MM:SS из uptime */
    uint64_t sec = timer_ticks() / 100;
    uint64_t hh = (sec / 3600) % 24, mm = (sec / 60) % 60, ss = sec % 60;
    char buf[32];
    char *p = buf;
    *p++ = (char)('0' + hh / 10); *p++ = (char)('0' + hh % 10); *p++ = ':';
    *p++ = (char)('0' + mm / 10); *p++ = (char)('0' + mm % 10); *p++ = ':';
    *p++ = (char)('0' + ss / 10); *p++ = (char)('0' + ss % 10); *p = 0;
    fb_rect(W - 120, ty + 4, 112, 28, 0x1A2332);
    fb_text(W - 100, ty + 10, buf, 0xE0E8F0, 2);
}

/* Полная перерисовка UI (без курсора) */
void draw_frame_content() {
    fb_restore();
    int act = active_window();
    for (int k = 0; k < nwin; k++) {
        int i = order[k];
        if (i >= 0 && i < nwin && wins[i] && wins[i]->visible)
            draw_window(i, i == act);
    }
    draw_start_menu();
    draw_taskbar();
    fb_present();
    cur_ox = -1000; /* курсор нужно нарисовать заново */
}

void handle_mouse(int mx, int my, bool left) {
    int W = fb_width(), H = fb_height();
    bool pressed = left && !prevLeft;

    if (pressed) {
        /* клик по меню Пуск */
        if (start_menu) {
            int my0 = H - TASKBAR - 200;
            if (mx >= 12 && mx < 196 && my >= my0 + 40 && my < my0 + 170) {
                int item = (my - (my0 + 40)) / 34;
                if (item == 0) {
                    int i = find_by_type(WT_TERM);
                    if (i >= 0) { wins[i]->visible = true; raise_window(i); }
                } else if (item == 1) {
                    int i = find_by_type(WT_ABOUT);
                    if (i >= 0) { wins[i]->visible = true; raise_window(i); }
                    else window_create(WT_ABOUT, "About RadiantOS", "About", 400, 120, 440, 250, true);
                } else if (item == 2) {
                    int i = find_by_type(WT_SYS);
                    if (i >= 0) { wins[i]->visible = true; raise_window(i); }
                } else if (item == 3) {
                    char title[32];
                    char *p = put_s(title, "Note #");
                    p = put_n(p, (uint64_t)next_id); *p = 0;
                    window_create(WT_NOTE, title, "Note", 60 + nwin * 20, 40 + nwin * 20, 360, 220, true);
                }
                start_menu = false;
                prevLeft = left;
                return;
            }
            if (!(mx >= 4 && mx < 104 && my >= H - TASKBAR))
                start_menu = false;
        }

        if (my >= H - TASKBAR) {
            if (mx >= 4 && mx < 104) {
                start_menu = !start_menu;
            } else if (mx >= 110 && mx < 180) {
                char title[32];
                char *p = put_s(title, "Note #");
                p = put_n(p, (uint64_t)next_id); *p = 0;
                window_create(WT_NOTE, title, "Note",
                              50 + (nwin % 6) * 28, 40 + (nwin % 6) * 28,
                              360, 220, true);
                start_menu = false;
            } else {
                int bx = 190;
                for (int i = 0; i < nwin && bx + 120 < W - 140; i++) {
                    if (!wins[i]) { bx += 116; continue; }
                    if (mx >= bx && mx < bx + 110) {
                        if (wins[i]->visible && i == active_window())
                            wins[i]->visible = false;
                        else {
                            wins[i]->visible = true;
                            raise_window(i);
                        }
                        break;
                    }
                    bx += 116;
                }
                start_menu = false;
            }
        } else {
            /* иконки рабочего стола */
            if (mx >= 24 && mx < 88 && my >= 24 && my < 88) {
                int i = find_by_type(WT_TERM);
                if (i >= 0) { wins[i]->visible = true; raise_window(i); }
            } else if (mx >= 24 && mx < 88 && my >= 110 && my < 174) {
                int i = find_by_type(WT_ABOUT);
                if (i >= 0) { wins[i]->visible = true; raise_window(i); }
                else window_create(WT_ABOUT, "About RadiantOS", "About", 400, 120, 440, 250, true);
            } else {
                for (int k = nwin - 1; k >= 0; k--) {
                    int i = order[k];
                    if (i < 0 || i >= nwin || !wins[i] || !wins[i]->visible) continue;
                    if (!wins[i]->inside(mx, my)) continue;
                    raise_window(i);
                    if (wins[i]->inClose(mx, my)) {
                        if (wins[i]->closable) window_destroy(i);
                        else wins[i]->visible = false;
                    } else if (wins[i]->inTitle(mx, my)) {
                        dragging = i;
                        dragDX = mx - wins[i]->x;
                        dragDY = my - wins[i]->y;
                    }
                    break;
                }
            }
            start_menu = false;
        }
    }
    if (!left) dragging = -1;
    else if (dragging >= 0 && dragging < nwin && wins[dragging]) {
        Window *w = wins[dragging];
        w->x = clampi(mx - dragDX, 0, W - w->w);
        w->y = clampi(my - dragDY, 0, H - TASKBAR - TB);
    }
    prevLeft = left;
}

} // namespace

static void term_write_hook(const char *data, size_t len) {
    /* выводим в терминал построчно */
    char line[128];
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        char ch = data[i];
        if (ch == '\n' || n >= sizeof(line) - 1) {
            line[n] = 0;
            if (n) t_print(line);
            n = 0;
            if (ch != '\n') line[n++] = ch;
        } else {
            line[n++] = ch;
        }
    }
    if (n) { line[n] = 0; t_print(line); }
}

extern "C" void gui_run(void) {
    kconsole_set_hook(term_write_hook);
    /* стартовые окна */
    window_create(WT_TERM,  "Terminal",        "Terminal", 40,  50, 580, 340, false);
    window_create(WT_ABOUT, "About RadiantOS", "About",    420, 140, 440, 250, true);
    window_create(WT_SYS,   "System",          "System",   100, 360, 420, 200, false);

    t_print("Welcome to RadiantOS!");
    t_print("Type help  |  win  |  mem");
    draw_desktop();
    draw_frame_content();

    unsigned lastSeq = mouse_seq() - 1;
    uint64_t lastSec = ~0ULL;
    bool dirty = true;
    int last_mx = -1, last_my = -1;

    for (;;) {
        int mx, my, mb;
        mouse_get(&mx, &my, &mb);
        unsigned seq = mouse_seq();
        bool mouse_moved = (seq != lastSeq);
        if (mouse_moved) {
            lastSeq = seq;
            bool was_drag = dragging >= 0;
            handle_mouse(mx, my, mb & 1);
            if (was_drag || dragging >= 0 || (mb & 1))
                dirty = true; /* клик / перетаскивание — полная перерисовка */
            else {
                /* только курсор — быстро */
                draw_cursor_on_screen(mx, my);
                last_mx = mx; last_my = my;
            }
        }

        char ch;
        while ((ch = keyboard_getchar()) != 0) {
            int ti = find_by_type(WT_TERM);
            if (ti >= 0 && wins[ti] && wins[ti]->visible)
                t_key(ch);
            dirty = true;
        }

        uint64_t sec = timer_ticks() / 100;
        if (sec != lastSec) { lastSec = sec; dirty = true; }

        if (dirty) {
            draw_frame_content();
            draw_cursor_on_screen(mx, my);
            dirty = false;
            last_mx = mx; last_my = my;
            continue;
        }

        __asm__ volatile("cli");
        if (mouse_seq() == lastSeq && !keyboard_pending() && timer_ticks() / 100 == lastSec)
            __asm__ volatile("sti; hlt");
        else
            __asm__ volatile("sti");
    }
}
