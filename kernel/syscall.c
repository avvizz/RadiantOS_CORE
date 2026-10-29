/* Обработчики syscall (int 0x80) */
#include "syscall.h"
#include "interrupts.h"
#include "gdt.h"
#include "task.h"
#include "kconsole.h"

extern void isr128(void);
extern void idt_set_gate(int n, uint64_t addr, uint8_t dpl);
extern void user_exit_to_kernel(uint64_t code);

void syscall_handler(struct regs *r) {
    uint64_t nr = r->rax;
    uint64_t a0 = r->rdi;
    uint64_t a1 = r->rsi;
    uint64_t a2 = r->rdx;
    int64_t  ret = 0;

    switch (nr) {
    case SYS_EXIT:
        /* Не возвращаемся в user — прыгаем обратно в enter_user_mode */
        user_exit_to_kernel(a0);
        /* unreachable */
        break;

    case SYS_WRITE: {
        const char *buf = (const char *)a1;
        uint64_t len = a2;
        if (a0 == 1 && buf && len < 0x10000) {
            kconsole_write(buf, (size_t)len);
            ret = (int64_t)len;
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

    default:
        ret = -1;
        break;
    }

    r->rax = (uint64_t)ret;
}

void syscall_init(void) {
    idt_set_gate(128, (uint64_t)isr128, 3);
}
