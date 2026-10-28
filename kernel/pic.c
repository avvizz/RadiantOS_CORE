/* Контроллер прерываний 8259 PIC */
#include "interrupts.h"
#include "io.h"

#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

void pic_init(void) {
    /* переносим IRQ 0..15 на векторы 32..47, чтобы не пересекаться с исключениями CPU */
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DAT, 0x20); io_wait();   /* master: вектор 32 */
    outb(PIC2_DAT, 0x28); io_wait();   /* slave: вектор 40 */
    outb(PIC1_DAT, 0x04); io_wait();
    outb(PIC2_DAT, 0x02); io_wait();
    outb(PIC1_DAT, 0x01); io_wait();
    outb(PIC2_DAT, 0x01); io_wait();
    outb(PIC1_DAT, 0xFF);              /* маскируем всё, драйверы включают нужное */
    outb(PIC2_DAT, 0xFF);
}

void pic_unmask(int irq) {
    uint16_t port = irq < 8 ? PIC1_DAT : PIC2_DAT;
    if (irq >= 8) { pic_unmask(2); irq -= 8; }   /* каскад master->slave */
    outb(port, inb(port) & ~(1 << irq));
}

void pic_eoi(int irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}
