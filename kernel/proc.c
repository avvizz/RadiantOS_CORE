#include "proc.h"
#include "task.h"
#include "elf.h"
#include "kmalloc.h"
#include "kstring.h"
#include "kconsole.h"

static struct process procs[PROC_MAX];
static int next_pid = 1;

struct spawn_arg {
    int  pid;
    char path[64];
};

void proc_init(void) {
    memset(procs, 0, sizeof(procs));
    next_pid = 1;
}

struct process *proc_get(int pid) {
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].state != PROC_FREE && procs[i].pid == pid)
            return &procs[i];
    return 0;
}

int proc_count(void) {
    int n = 0;
    for (int i = 0; i < PROC_MAX; i++)
        if (procs[i].state == PROC_RUNNING) n++;
    return n;
}

void proc_mark_exit(int pid, int code) {
    struct process *p = proc_get(pid);
    if (!p) return;
    p->exit_code = code;
    p->state = PROC_ZOMBIE;

    char buf[64];
    char *q = buf;
    const char *s = "[proc ";
    while (*s) *q++ = *s++;
    /* pid */
    char t[12]; int n = 0; int v = pid;
    if (v == 0) t[n++] = '0';
    else { int neg = 0; if (v < 0) { neg = 1; v = -v; }
           while (v) { t[n++] = '0' + v % 10; v /= 10; }
           if (neg) t[n++] = '-'; }
    while (n) *q++ = t[--n];
    s = "] exit ";
    while (*s) *q++ = *s++;
    v = code; n = 0;
    if (v == 0) t[n++] = '0';
    else { while (v) { t[n++] = '0' + v % 10; v /= 10; } }
    while (n) *q++ = t[--n];
    *q++ = '\n'; *q = 0;
    kconsole_write(buf, (size_t)(q - buf));
}

static void spawn_worker(void *arg) {
    struct spawn_arg *sa = (struct spawn_arg *)arg;
    int pid = sa->pid;
    char path[64];
    size_t i = 0;
    while (sa->path[i] && i < 63) { path[i] = sa->path[i]; i++; }
    path[i] = 0;
    kfree(sa);

    int code = elf_run(path);
    proc_mark_exit(pid, code);
    task_exit();
}

int proc_spawn(const char *path) {
    if (!path || !path[0]) return -1;

    struct process *p = 0;
    for (int i = 0; i < PROC_MAX; i++) {
        if (procs[i].state == PROC_FREE || procs[i].state == PROC_ZOMBIE) {
            p = &procs[i];
            break;
        }
    }
    if (!p) return -1;

    struct spawn_arg *sa = (struct spawn_arg *)kmalloc(sizeof(*sa));
    if (!sa) return -1;
    memset(sa, 0, sizeof(*sa));

    int pid = next_pid++;
    sa->pid = pid;
    size_t n = 0;
    while (path[n] && n < 63) { sa->path[n] = path[n]; n++; }
    sa->path[n] = 0;

    memset(p, 0, sizeof(*p));
    p->pid = pid;
    p->state = PROC_RUNNING;
    n = 0;
    while (path[n] && n < PROC_NAME_MAX - 1) { p->name[n] = path[n]; n++; }
    p->name[n] = 0;

    uint64_t tid = task_create(p->name, spawn_worker, sa);
    if (tid == (uint64_t)-1) {
        p->state = PROC_FREE;
        kfree(sa);
        return -1;
    }
    p->task_id = tid;
    return pid;
}
