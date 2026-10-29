#include "kconsole.h"
#include "io.h"

static kconsole_hook_t hook = 0;

void kconsole_set_hook(kconsole_hook_t h) {
    hook = h;
}

void kconsole_write(const char *data, size_t len) {
    if (!data || !len) return;

    /* serial always */
    for (size_t i = 0; i < len; i++) {
        while (!(inb(0x3F8 + 5) & 0x20)) {}
        outb(0x3F8, (uint8_t)data[i]);
    }

    if (hook)
        hook(data, len);
}
