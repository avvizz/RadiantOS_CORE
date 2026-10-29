/* Минимальный загрузчик ELF64 (x86-64, ET_EXEC / ET_DYN).
   Пока запускает entry в kernel mode (без ring3). */

#include "elf.h"
#include "fs.h"
#include "kmalloc.h"
#include "pmm.h"
#include "kstring.h"
#include "syscall.h"
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

    uint64_t min_va = ~0ULL, max_va = 0;
    int load_count = 0;
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        uint64_t start = ph[i].p_vaddr;
        uint64_t end   = ph[i].p_vaddr + ph[i].p_memsz;
        if (start < min_va) min_va = start;
        if (end > max_va) max_va = end;
        load_count++;
    }
    if (load_count == 0 || min_va >= max_va) {
        kfree(img);
        return -2;
    }

    uint64_t span = max_va - min_va;
    int use_direct = (eh->e_type == ET_EXEC &&
                      min_va >= 0x100000 &&
                      max_va <= 0x100000000ULL);

    uint8_t *base_mem = 0;
    uint64_t load_bias = 0;
    size_t pages = (span + 0xFFF) / 0x1000;
    if (pages == 0) pages = 1;

    if (use_direct) {
        for (uint16_t i = 0; i < eh->e_phnum; i++) {
            if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
            uint64_t a = ph[i].p_vaddr & ~0xFFFULL;
            uint64_t e = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFF) & ~0xFFFULL;
            pmm_reserve(a, e - a);
        }
        load_bias = 0;
    } else {
        base_mem = (uint8_t *)pmm_alloc_pages(pages);
        if (!base_mem) {
            kfree(img);
            return -1;
        }
        memset(base_mem, 0, pages * 0x1000);
        load_bias = (uint64_t)base_mem - min_va;
    }

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_offset + ph[i].p_filesz > fsz) {
            if (base_mem) pmm_free_pages(base_mem, pages);
            kfree(img);
            return -1;
        }
        uint8_t *dst = (uint8_t *)(ph[i].p_vaddr + load_bias);
        if (ph[i].p_filesz)
            memcpy(dst, img + ph[i].p_offset, (size_t)ph[i].p_filesz);
        if (ph[i].p_memsz > ph[i].p_filesz)
            memset(dst + ph[i].p_filesz, 0,
                   (size_t)(ph[i].p_memsz - ph[i].p_filesz));
    }

    uint64_t entry = eh->e_entry + load_bias;
    kfree(img);

    /* User stack: 16 KiB */
    void *ustack = pmm_alloc_pages(4);
    if (!ustack) {
        if (base_mem) pmm_free_pages(base_mem, pages);
        return -1;
    }
    uint64_t user_sp = (uint64_t)ustack + 4 * 4096;

    /* Kernel stack for syscall (TSS.RSP0) */
    void *kstack = pmm_alloc_pages(4);
    if (kstack)
        tss_set_kernel_stack((uint64_t)kstack + 4 * 4096);

    /* ring 3 → после SYS_EXIT возвращаемся сюда */
    uint64_t code = enter_user_mode(entry, user_sp);

    if (base_mem)
        pmm_free_pages(base_mem, pages);
    if (ustack)
        pmm_free_pages(ustack, 4);
    /* kstack оставляем — может ещё понадобиться; можно free позже */

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
    if (!buf) return;

    /* Общий заголовок DYN, entry=0x1000, один PT_LOAD */
    /* Патчим p_filesz/p_memsz на 0x2000 чтобы код+данные влезли */
    memset(buf, 0, 0x2000);
    memcpy(buf, tiny_elf, sizeof(tiny_elf));
    /* phdr at offset 0x40: p_filesz @ +32, p_memsz @ +40 */
    {
        uint64_t sz = 0x2000;
        memcpy(buf + 0x40 + 32, &sz, 8);  /* p_filesz */
        memcpy(buf + 0x40 + 40, &sz, 8);  /* p_memsz */
    }

    /* ---- tiny.elf: exit(0) ---- */
    {
        static const uint8_t code[] = {
            0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00, /* mov $0,%rax */
            0x48, 0xC7, 0xC7, 0x00, 0x00, 0x00, 0x00, /* mov $0,%rdi */
            0xCD, 0x80                                /* int $0x80 */
        };
        memcpy(buf + 0x1000, code, sizeof(code));
        fs_write("tiny.elf", buf, 0x2000);
    }

    /* ---- hello.elf: write + exit ---- */
    memset(buf + 0x1000, 0, 0x1000);
    {
        static const char msg[] = "Hello from user mode!\n";
        memcpy(buf + 0x1050, msg, sizeof(msg) - 1);

        /* lea: next_rip=0x1015, msg=0x1050 → disp=0x3B */
        static const uint8_t code[] = {
            0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, /* mov $1,%rax  WRITE */
            0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, /* mov $1,%rdi  fd */
            0x48, 0x8D, 0x35, 0x3B, 0x00, 0x00, 0x00, /* lea 0x3B(%rip),%rsi */
            0x48, 0xC7, 0xC2, 0x16, 0x00, 0x00, 0x00, /* mov $22,%rdx */
            0xCD, 0x80,                               /* int $0x80 */
            0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00, /* mov $0,%rax  EXIT */
            0x48, 0x31, 0xFF,                         /* xor %rdi,%rdi */
            0xCD, 0x80                                /* int $0x80 */
        };
        memcpy(buf + 0x1000, code, sizeof(code));
        fs_write("hello.elf", buf, 0x2000);
    }

    kfree(buf);
}
