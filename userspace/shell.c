/* Простой user-space shell для RadiantOS */
#include "syscalls.h"

static size_t u_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static int u_streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int u_starts(const char *s, const char *p) {
    while (*p) { if (*s++ != *p++) return 0; }
    return 1;
}
static void print(const char *s) { sys_write(1, s, u_strlen(s)); }

/* stdin через ядро пока заглушка — shell читает команды из... 
   Для MVP: shell демонстрирует spawn/list/time и завершается циклом
   с prompt; ввод идёт через kernel Terminal командой `shell` которая
   перенаправляет строки. Упрощение: shell работает в batch-режиме
   из argv... нет argv.

   Практичный вариант: shell.elf печатает help и список файлов,
   умеет spawn hello.elf, sleep, time — без интерактивного ввода.
   Интерактив оставим в kernel Terminal, а shell покажем как user app.
*/

void _start(void) {
    print("RadiantOS user shell\n");
    print("Built-in demo (no TTY input yet):\n");

    char list[256];
    long n = sys_list(list, sizeof(list));
    if (n > 0) {
        print("Files:\n");
        print(list);
    }

    print("Spawning hello.elf...\n");
    long pid = sys_spawn("hello.elf");
    if (pid < 0) print("spawn failed\n");
    else print("spawned ok\n");

    print("Sleep 1s...\n");
    sys_sleep(100); /* 100 ticks @ 100Hz = 1s */

    long t = sys_time();
    print("Time(s): ");
    /* print number */
    char buf[16];
    int i = 0;
    long v = t;
    if (v == 0) buf[i++] = '0';
    else {
        char tmp[16]; int k = 0;
        while (v) { tmp[k++] = '0' + (v % 10); v /= 10; }
        while (k) buf[i++] = tmp[--k];
    }
    buf[i++] = '\n';
    sys_write(1, buf, i);

    print("Shell done.\n");
    sys_exit(0);
}
