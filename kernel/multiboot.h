#ifndef MULTIBOOT_H
#define MULTIBOOT_H
#include <stdint.h>

#define MB2_MAGIC 0x36D76289

#define MB2_TAG_END            0
#define MB2_TAG_CMDLINE        1
#define MB2_TAG_BOOT_LOADER    2
#define MB2_TAG_MODULE         3
#define MB2_TAG_BASIC_MEMINFO  4
#define MB2_TAG_BOOTDEV        5
#define MB2_TAG_MMAP           6
#define MB2_TAG_VBE            7
#define MB2_TAG_FRAMEBUFFER    8
#define MB2_TAG_ELF_SECTIONS   9
#define MB2_TAG_APM            10
#define MB2_TAG_EFI32          11
#define MB2_TAG_EFI64          12
#define MB2_TAG_SMBIOS         13
#define MB2_TAG_ACPI_OLD       14
#define MB2_TAG_ACPI_NEW       15
#define MB2_TAG_NETWORK        16
#define MB2_TAG_EFI_MMAP       17
#define MB2_TAG_EFI_BS         18
#define MB2_TAG_EFI_IH32       19
#define MB2_TAG_EFI_IH64       20
#define MB2_TAG_LOAD_BASE      21

#define MB2_FB_TYPE_INDEXED    0
#define MB2_FB_TYPE_RGB        1
#define MB2_FB_TYPE_EGA_TEXT   2

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint16_t reserved;
    /* followed by color info depending on type */
} __attribute__((packed));

/* RGB color field info (after the common part when fb_type == RGB) */
struct mb2_fb_rgb {
    uint8_t red_pos;
    uint8_t red_mask_size;
    uint8_t green_pos;
    uint8_t green_mask_size;
    uint8_t blue_pos;
    uint8_t blue_mask_size;
} __attribute__((packed));

#ifdef __cplusplus
extern "C" {
#endif

/* Parse Multiboot2 info and initialize graphics if possible.
   Returns 1 if a usable RGB/indexed framebuffer was found, 0 otherwise. */
int mb2_init_framebuffer(unsigned magic, unsigned long info_addr);

#ifdef __cplusplus
}
#endif
#endif
