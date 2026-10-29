#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int elf_run(const char *filename);

typedef void (*elf_info_cb)(const char *line);
int elf_info(const char *filename, elf_info_cb cb);

void elf_install_demos(void);

#ifdef __cplusplus
}
#endif

#endif
