#include "syscall.h"
#include "interrupts.h"
#include "gdt.h"
#include "task.h"
#include "kconsole.h"
#include "fs.h"
#include "proc.h"
#include "kmalloc.h"
#include "kstring.h"

extern void isr128(void);
extern void idt_set_gate(int n, uint64_t addr, uint8_t dpl);
extern void user_exit_to_kernel(uint64_t code);
extern uint64_t timer_ticks(void);

#define FD_MAX 16
struct fd_entry {
    int used;
    char name[32];
    size_t pos;
};
static struct fd_entry fds[FD_MAX];

static int fd_alloc(const char *name) {
    for (int i = 3; i < FD_MAX; i++) {
        if (!fds[i].used) {
            fds[i].used = 1;
            size_t n = 0;
            while (name[n] && n < 31) { fds[i].name[n] = name[n]; n++; }
            fds[i].name[n] = 0;
            fds[i].pos = 0;
            return i;
        }
    }
    return -1;
}

static char *list_buf;
static size_t list_pos, list_max;
static void list_cb(const char *name, size_t size, void *user) {
    (void)size; (void)user;
    if (!list_buf) return;
    size_t n = 0;
    while (name[n]) n++;
    if (list_pos + n + 2 >= list_max) return;
    for (size_t i = 0; i < n; i++) list_buf[list_pos++] = name[i];
    list_buf[list_pos++] = '\n';
    list_buf[list_pos] = 0;
}

void syscall_handler(struct regs *r) {
    uint64_t nr = r->rax;
    uint64_t a0 = r->rdi;
    uint64_t a1 = r->rsi;
    uint64_t a2 = r->rdx;
    int64_t  ret = 0;

    switch (nr) {
    case SYS_EXIT:
        user_exit_to_kernel(a0);
        break;

    case SYS_WRITE: {
        const char *buf = (const char *)a1;
        uint64_t len = a2;
        if ((a0 == 1 || a0 == 2) && buf && len < 0x10000) {
            kconsole_write(buf, (size_t)len);
            ret = (int64_t)len;
        } else if (a0 >= 3 && a0 < FD_MAX && fds[a0].used && buf && len < 0x10000) {
            ret = (fs_append(fds[a0].name, buf, (size_t)len) == 0) ? (int64_t)len : -1;
        } else {
            ret = -1;
        }
        break;
    }

    case SYS_YIELD:
        task_yield();
        ret = 0;
        break;

    case SYS_GETPID:
        ret = task_current() ? (int64_t)task_current()->id : 1;
        break;

    case SYS_OPEN: {
        const char *path = (const char *)a0;
        if (!path) { ret = -1; break; }
        if (!fs_find(path)) {
            if ((a1 & 1) == 0) { ret = -1; break; }
            if (fs_create(path) != 0) { ret = -1; break; }
        }
        ret = fd_alloc(path);
        break;
    }

    case SYS_READ: {
        char *buf = (char *)a1;
        uint64_t len = a2;
        if (!buf || len == 0 || len > 0x10000) { ret = -1; break; }
        if (a0 == 0) {
            ret = 0; /* stdin stub */
        } else if (a0 >= 3 && a0 < FD_MAX && fds[a0].used) {
            size_t fsz = fs_size(fds[a0].name);
            if (fsz == (size_t)-1) { ret = -1; break; }
            if (fds[a0].pos >= fsz) { ret = 0; break; }
            size_t avail = fsz - fds[a0].pos;
            size_t n = avail < len ? avail : (size_t)len;
            void *full = kmalloc(fsz);
            if (!full) { ret = -1; break; }
            int got = fs_read(fds[a0].name, full, fsz);
            if (got < 0) { kfree(full); ret = -1; break; }
            memcpy(buf, (uint8_t *)full + fds[a0].pos, n);
            fds[a0].pos += n;
            kfree(full);
            ret = (int64_t)n;
        } else {
            ret = -1;
        }
        break;
    }

    case SYS_CLOSE:
        if (a0 >= 3 && a0 < FD_MAX && fds[a0].used) {
            fds[a0].used = 0;
            ret = 0;
        } else if (a0 <= 2) ret = 0;
        else ret = -1;
        break;

    case SYS_SLEEP: {
        /* busy-wait: нельзя task_switch со стека syscall */
        uint64_t target = timer_ticks() + a0;
        while (timer_ticks() < target)
            __asm__ volatile("pause");
        ret = 0;
        break;
    }

    case SYS_TIME:
        ret = (int64_t)(timer_ticks() / 100);
        break;

    case SYS_SPAWN:
        ret = a0 ? proc_spawn((const char *)a0) : -1;
        break;

    case SYS_LIST: {
        list_buf = (char *)a0;
        list_max = (size_t)a1;
        list_pos = 0;
        if (!list_buf || list_max < 2) { ret = -1; break; }
        list_buf[0] = 0;
        fs_list(list_cb, 0);
        ret = (int64_t)list_pos;
        break;
    }

    default:
        ret = -1;
        break;
    }

    r->rax = (uint64_t)ret;
}

void syscall_init(void) {
    for (int i = 0; i < FD_MAX; i++) fds[i].used = 0;
    idt_set_gate(128, (uint64_t)isr128, 3);
}
