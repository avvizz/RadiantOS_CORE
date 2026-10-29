/* Минимальный загрузчик ELF64 (x86-64, ET_EXEC / ET_DYN).
   Пока запускает entry в kernel mode (без ring3). */

#include "elf.h"
#include "fs.h"
#include "kmalloc.h"
#include "pmm.h"
#include "kstring.h"
#include "syscall.h"
#include "vmm.h"
#include "user_hello_blob.h"
#include "gdt.h"

#define EI_NIDENT 16

#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define EV_CURRENT  1

#define ET_EXEC 2
#define ET_DYN  3

#define EM_X86_64 62

#define PT_LOAD 1

struct elf64_ehdr {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed));

static void hex64(char *d, uint64_t v) {
    const char *h = "0123456789ABCDEF";
    d[0] = '0'; d[1] = 'x';
    for (int i = 0; i < 16; i++)
        d[2 + i] = h[(v >> (60 - 4 * i)) & 0xF];
    d[18] = 0;
}

static void decu(char *d, uint64_t v) {
    char t[24]; int n = 0;
    if (v == 0) { d[0] = '0'; d[1] = 0; return; }
    while (v) { t[n++] = '0' + (v % 10); v /= 10; }
    int i = 0;
    while (n) d[i++] = t[--n];
    d[i] = 0;
}

static int check_header(const struct elf64_ehdr *eh) {
    if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
        eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3)
        return -1;
    if (eh->e_ident[4] != ELFCLASS64) return -2;
    if (eh->e_ident[5] != ELFDATA2LSB) return -2;
    if (eh->e_machine != EM_X86_64) return -2;
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) return -2;
    if (eh->e_phentsize < sizeof(struct elf64_phdr)) return -2;
    if (eh->e_phnum == 0 || eh->e_phnum > 64) return -2;
    return 0;
}

static uint8_t *load_file(const char *name, size_t *out_size) {
    size_t sz = fs_size(name);
    if (sz == (size_t)-1 || sz < sizeof(struct elf64_ehdr))
        return 0;
    uint8_t *buf = (uint8_t *)kmalloc(sz);
    if (!buf) return 0;
    int n = fs_read(name, buf, sz);
    if (n < 0 || (size_t)n != sz) {
        kfree(buf);
        return 0;
    }
    *out_size = sz;
    return buf;
}

int elf_info(const char *filename, elf_info_cb cb) {
    if (!cb) return -1;
    size_t fsz = 0;
    uint8_t *img = load_file(filename, &fsz);
    if (!img) { cb("Cannot read file"); return -1; }

    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)img;
    int rc = check_header(eh);
    if (rc != 0) {
        cb(rc == -1 ? "Not an ELF file" : "Unsupported ELF");
        kfree(img);
        return rc;
    }

    char line[96];
    char tmp[24];

    if (eh->e_type == ET_EXEC)
        cb("ELF64 x86-64 EXEC");
    else
        cb("ELF64 x86-64 DYN");

    hex64(tmp, eh->e_entry);
    char *p = line;
    const char *s = "entry ";
    while (*s) *p++ = *s++;
    s = tmp; while (*s) *p++ = *s++;
    *p = 0;
    cb(line);

    decu(tmp, eh->e_phnum);
    p = line;
    s = "phnum ";
    while (*s) *p++ = *s++;
    s = tmp; while (*s) *p++ = *s++;
    *p = 0;
    cb(line);

    const struct elf64_phdr *ph =
        (const struct elf64_phdr *)(img + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        p = line;
        s = "LOAD va=";
        while (*s) *p++ = *s++;
        hex64(tmp, ph[i].p_vaddr);
        s = tmp; while (*s) *p++ = *s++;
        s = " sz=";
        while (*s) *p++ = *s++;
        hex64(tmp, ph[i].p_filesz);
        s = tmp; while (*s) *p++ = *s++;
        *p = 0;
        cb(line);
    }

    kfree(img);
    return 0;
}


int elf_run(const char *filename) {
    size_t fsz = 0;
    uint8_t *img = load_file(filename, &fsz);
    if (!img) return -1;

    const struct elf64_ehdr *eh = (const struct elf64_ehdr *)img;
    int rc = check_header(eh);
    if (rc != 0) {
        kfree(img);
        return rc;
    }

    const struct elf64_phdr *ph =
        (const struct elf64_phdr *)(img + eh->e_phoff);

    addrspace_t as = vmm_create_as();
    if (!as) {
        kfree(img);
        return -1;
    }

    /* Загружаем PT_LOAD в физические страницы и маппим в user AS */
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        if (ph[i].p_offset + ph[i].p_filesz > fsz) {
            vmm_destroy_as(as);
            kfree(img);
            return -1;
        }

        uint64_t va = ph[i].p_vaddr & ~0xFFFULL;
        uint64_t end = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFFULL;
        size_t pages = (size_t)((end - va) / 4096);
        if (pages == 0) pages = 1;

        void *phys = pmm_alloc_pages(pages);
        if (!phys) {
            vmm_destroy_as(as);
            kfree(img);
            return -1;
        }
        memset(phys, 0, pages * 4096);

        uint64_t off = ph[i].p_vaddr - va;
        if (ph[i].p_filesz)
            memcpy((uint8_t *)phys + off, img + ph[i].p_offset, (size_t)ph[i].p_filesz);

        uint64_t flags = PTE_WRITE | PTE_USER;
        if (vmm_map_range(as, va, (uint64_t)phys, pages, flags) != 0) {
            pmm_free_pages(phys, pages);
            vmm_destroy_as(as);
            kfree(img);
            return -1;
        }
    }

    /* User stack: 16 KiB at 0x7FFF0000 */
    const uint64_t stack_top = 0x7FFF0000ULL;
    const size_t stack_pages = 4;
    void *ustack = pmm_alloc_pages(stack_pages);
    if (!ustack) {
        vmm_destroy_as(as);
        kfree(img);
        return -1;
    }
    memset(ustack, 0, stack_pages * 4096);
    if (vmm_map_range(as, stack_top - stack_pages * 4096, (uint64_t)ustack,
                      stack_pages, PTE_WRITE | PTE_USER) != 0) {
        pmm_free_pages(ustack, stack_pages);
        vmm_destroy_as(as);
        kfree(img);
        return -1;
    }

    void *kstack = pmm_alloc_pages(4);
    if (kstack)
        tss_set_kernel_stack((uint64_t)kstack + 4 * 4096);

    uint64_t entry = eh->e_entry;
    kfree(img);

    vmm_switch(as);
    uint64_t code = enter_user_mode(entry, stack_top);
    vmm_switch(vmm_kernel_as());

    return (int)code;
}


/* Крошечный ELF64 DYN: один PT_LOAD, код = ret (0xC3).
   Можно запускать: run tiny.elf */
static const uint8_t tiny_elf[] = {
    /* e_ident */
    0x7F,'E','L','F', 2,1,1,0, 0,0,0,0,0,0,0,0,
    /* e_type=DYN, e_machine=X86_64, e_version=1 */
    0x03,0x00, 0x3E,0x00, 0x01,0x00,0x00,0x00,
    /* e_entry = 0x1000 (offset in VA space of segment) */
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
    /* e_phoff = 64 */
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* e_shoff = 0 */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* e_flags, e_ehsize=64, e_phentsize=56, e_phnum=1 */
    0x00,0x00,0x00,0x00, 0x40,0x00, 0x38,0x00, 0x01,0x00,
    /* e_shentsize, e_shnum, e_shstrndx */
    0x00,0x00, 0x00,0x00, 0x00,0x00,
    /* ---- PHDR ---- */
    /* p_type=PT_LOAD */
    0x01,0x00,0x00,0x00,
    /* p_flags=RX */
    0x05,0x00,0x00,0x00,
    /* p_offset = 0 */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* p_vaddr = 0 */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* p_paddr = 0 */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* p_filesz = 0x1001 (header + code at 0x1000) */
    0x01,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
    /* p_memsz  = 0x1001 */
    0x01,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
    /* p_align  = 0x1000 */
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,
};

/* Дополняем нулями до 0x1000 и пишем ret */



void elf_install_demos(void) {
    uint8_t *buf = (uint8_t *)kmalloc(0x2000);
    if (buf) {
        memset(buf, 0, 0x2000);
        memcpy(buf, tiny_elf, sizeof(tiny_elf));
        uint64_t sz = 0x2000;
        memcpy(buf + 0x40 + 32, &sz, 8);
        memcpy(buf + 0x40 + 40, &sz, 8);
        static const uint8_t code[] = {
            0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00,
            0x48, 0xC7, 0xC7, 0x00, 0x00, 0x00, 0x00,
            0xCD, 0x80
        };
        memcpy(buf + 0x1000, code, sizeof(code));
        fs_write("tiny.elf", buf, 0x2000);
        kfree(buf);
    }

    fs_write("hello.elf", user_hello_elf, user_hello_elf_len);
}
