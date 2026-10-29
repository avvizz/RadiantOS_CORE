/* Простой RAM-диск: плоский список файлов в kmalloc-памяти */
#include "fs.h"
#include "kmalloc.h"
#include "kstring.h"

static struct fs_file files[FS_MAX_FILES];
static int file_count = 0;

static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int name_ok(const char *name) {
    if (!name || !*name) return 0;
    size_t n = strlen(name);
    if (n == 0 || n > FS_NAME_MAX) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (c <= 32 || c >= 127) return 0;
        if (c == '/' || c == '\\' || c == ':' || c == '*') return 0;
    }
    return 1;
}

struct fs_file *fs_find(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name))
            return &files[i];
    }
    return 0;
}

int fs_create(const char *name) {
    if (!name_ok(name)) return -1;
    if (fs_find(name)) return -1;

    struct fs_file *f = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) { f = &files[i]; break; }
    }
    if (!f) return -1;

    memset(f, 0, sizeof(*f));
    size_t n = strlen(name);
    memcpy(f->name, name, n);
    f->name[n] = 0;
    f->used = 1;
    file_count++;
    return 0;
}

int fs_delete(const char *name) {
    struct fs_file *f = fs_find(name);
    if (!f) return -1;
    if (f->data) kfree(f->data);
    memset(f, 0, sizeof(*f));
    file_count--;
    return 0;
}

static int ensure_cap(struct fs_file *f, size_t need) {
    if (need > FS_MAX_SIZE) return -1;
    if (f->capacity >= need) return 0;

    size_t cap = f->capacity ? f->capacity : 256;
    while (cap < need) cap *= 2;
    if (cap > FS_MAX_SIZE) cap = FS_MAX_SIZE;
    if (need > cap) return -1;

    uint8_t *n = (uint8_t *)kmalloc(cap);
    if (!n) return -1;
    if (f->data && f->size)
        memcpy(n, f->data, f->size);
    if (f->data) kfree(f->data);
    f->data = n;
    f->capacity = cap;
    return 0;
}

int fs_write(const char *name, const void *data, size_t len) {
    struct fs_file *f = fs_find(name);
    if (!f) {
        if (fs_create(name) != 0) return -1;
        f = fs_find(name);
        if (!f) return -1;
    }
    if (len == 0) {
        f->size = 0;
        return 0;
    }
    if (ensure_cap(f, len) != 0) return -1;
    memcpy(f->data, data, len);
    f->size = len;
    return 0;
}

int fs_append(const char *name, const void *data, size_t len) {
    struct fs_file *f = fs_find(name);
    if (!f) {
        if (fs_create(name) != 0) return -1;
        f = fs_find(name);
        if (!f) return -1;
    }
    if (len == 0) return 0;
    if (ensure_cap(f, f->size + len) != 0) return -1;
    memcpy(f->data + f->size, data, len);
    f->size += len;
    return 0;
}

int fs_read(const char *name, void *buf, size_t max) {
    struct fs_file *f = fs_find(name);
    if (!f) return -1;
    size_t n = f->size < max ? f->size : max;
    if (n && buf) memcpy(buf, f->data, n);
    return (int)n;
}

size_t fs_size(const char *name) {
    struct fs_file *f = fs_find(name);
    if (!f) return (size_t)-1;
    return f->size;
}

void fs_list(fs_list_cb cb, void *user) {
    if (!cb) return;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used)
            cb(files[i].name, files[i].size, user);
    }
}

int fs_count(void) { return file_count; }

void fs_init(void) {
    memset(files, 0, sizeof(files));
    file_count = 0;

    static const char readme[] =
        "RadiantOS RAM disk\n"
        "Commands: ls cat write rm touch\n"
        "Try: cat readme.txt\n";
    fs_write("readme.txt", readme, sizeof(readme) - 1);

    static const char hello[] = "Hello from RadiantOS filesystem!\n";
    fs_write("hello.txt", hello, sizeof(hello) - 1);

    static const char version[] = "RadiantOS 0.3\n";
    fs_write("version", version, sizeof(version) - 1);
}
