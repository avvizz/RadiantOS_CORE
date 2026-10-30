/* Initrd: простой формат RIRD
   magic[4]="RIRD", count u32 LE,
   then for each: namelen u16, name[namelen], size u32, data[size] */

#include "initrd.h"
#include "fs.h"
#include "kstring.h"

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char     cmdline[0];
};

static int load_blob(const uint8_t *data, uint32_t len) {
    if (len < 8) return -1;
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'R' || data[3] != 'D')
        return -1;
    uint32_t count = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    uint32_t off = 8;
    for (uint32_t i = 0; i < count && off + 2 < len; i++) {
        uint16_t nlen = data[off] | (data[off + 1] << 8);
        off += 2;
        if (off + nlen + 4 > len) break;
        char name[64];
        uint16_t cn = nlen < 63 ? nlen : 63;
        for (uint16_t k = 0; k < cn; k++) name[k] = (char)data[off + k];
        name[cn] = 0;
        off += nlen;
        uint32_t sz = data[off] | (data[off + 1] << 8) |
                      (data[off + 2] << 16) | (data[off + 3] << 24);
        off += 4;
        if (off + sz > len) break;
        fs_write(name, data + off, sz);
        off += sz;
    }
    return 0;
}

void initrd_load(unsigned magic, unsigned mbinfo) {
    if (magic != 0x36D76289) return;
    uint8_t *ptr = (uint8_t *)(uintptr_t)mbinfo;
    uint32_t total = *(uint32_t *)ptr;
    uint8_t *end = ptr + total;
    ptr += 8; /* skip total_size + reserved */

    while (ptr + 8 <= end) {
        struct mb2_tag *tag = (struct mb2_tag *)ptr;
        if (tag->type == 0) break;
        if (tag->type == 3 && tag->size >= 16) {
            struct mb2_module *mod = (struct mb2_module *)ptr;
            uint32_t mlen = mod->mod_end - mod->mod_start;
            load_blob((const uint8_t *)(uintptr_t)mod->mod_start, mlen);
        }
        uint32_t sz = (tag->size + 7) & ~7u;
        if (sz == 0) break;
        ptr += sz;
    }
}
