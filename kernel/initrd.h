#ifndef INITRD_H
#define INITRD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Распаковать initrd из multiboot modules (или встроенный fallback) */
void initrd_load(unsigned magic, unsigned mbinfo);

#ifdef __cplusplus
}
#endif

#endif
