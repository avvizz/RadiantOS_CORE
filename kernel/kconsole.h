#ifndef KCONSOLE_H
#define KCONSOLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*kconsole_hook_t)(const char *data, size_t len);

void kconsole_set_hook(kconsole_hook_t hook);
void kconsole_write(const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
