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

void idt_init(void);                                  /* таблица прерываний */
void irq_install_handler(int irq, irq_handler_t fn);  /* обработчик IRQ 0..15 */

void pic_init(void);                                  /* контроллер прерываний */
void pic_unmask(int irq);
void pic_eoi(int irq);

void timer_init(uint32_t hz);                         /* PIT */
uint64_t timer_ticks(void);

void keyboard_init(void);                             /* PS/2 клавиатура */
char keyboard_getchar(void);                          /* 0, если буфер пуст */
int  keyboard_pending(void);
void keyboard_set_echo(int on);                       /* печатать ввод в текстовом режиме */

void mouse_init(void);                                /* PS/2 мышь */
void mouse_set_bounds(int w, int h);
void mouse_get(int *x, int *y, int *buttons);
unsigned mouse_seq(void);                             /* растёт при каждом движении */

#ifdef __cplusplus
}
#endif
#endif
