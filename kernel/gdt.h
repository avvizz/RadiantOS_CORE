#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Селекторы */
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18   /* + RPL 3 = 0x1B */
#define GDT_USER_DATA    0x20   /* + RPL 3 = 0x23 */
#define GDT_TSS          0x28

void gdt_init(void);
void tss_set_kernel_stack(uint64_t rsp0);

#ifdef __cplusplus
}
#endif

#endif
