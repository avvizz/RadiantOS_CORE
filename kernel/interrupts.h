#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Состояние регистров, которое сохраняет isr.S (порядок важен!) */
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

typedef void (*irq_handler_t)(struct regs *);

void idt_init(void);
void irq_install_handler(int irq, irq_handler_t fn);

void pic_init(void);
void pic_unmask(int irq);
void pic_eoi(int irq);

void timer_init(uint32_t hz);
uint64_t timer_ticks(void);
int  timer_need_resched(void);   /* 1, если таймер просит отдать CPU */

void keyboard_init(void);
char keyboard_getchar(void);
int  keyboard_pending(void);
void keyboard_set_echo(int on);

void mouse_init(void);
void mouse_set_bounds(int w, int h);
void mouse_get(int *x, int *y, int *buttons);
unsigned mouse_seq(void);

#ifdef __cplusplus
}
#endif
#endif
