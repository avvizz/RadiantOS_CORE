/* Multiboot2 info parser for AwizzOS */
#include "multiboot.h"
#include "gfx.h"

int mb2_init_framebuffer(unsigned magic, unsigned long info_addr) {
    if (magic != MB2_MAGIC || !info_addr)
        return 0;

    /* First 8 bytes: total_size + reserved */
    uint32_t *header = (uint32_t *)info_addr;
    uint32_t total_size = header[0];
    if (total_size < 16) return 0;

    struct mb2_tag *tag = (struct mb2_tag *)(info_addr + 8);
    while ((unsigned long)tag < info_addr + total_size) {
        if (tag->type == MB2_TAG_END)
            break;
        if (tag->type == MB2_TAG_FRAMEBUFFER) {
            struct mb2_tag_framebuffer *fb =
                (struct mb2_tag_framebuffer *)tag;
            /* We need RGB or indexed with usable bpp */
            if (fb->fb_type == MB2_FB_TYPE_RGB ||
                fb->fb_type == MB2_FB_TYPE_INDEXED) {
                if (fb->bpp == 32 || fb->bpp == 24) {
                    gfx_init((uint8_t *)(uintptr_t)fb->addr,
                             fb->width, fb->height,
                             fb->pitch, fb->bpp);
                    return gfx_ready;
                }
            }
            /* EGA text – not graphics */
        }
        /* next tag (8-byte aligned) */
        uint32_t sz = tag->size;
        if (sz < 8) break;
        tag = (struct mb2_tag *)((uint8_t *)tag + ((sz + 7) & ~7));
    }
    return 0;
}
