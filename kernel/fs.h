#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FS_MAX_FILES   32
#define FS_NAME_MAX    28
#define FS_MAX_SIZE    (256 * 1024)  /* макс. размер одного файла */

struct fs_file {
    char     name[FS_NAME_MAX + 1];
    uint8_t *data;
    size_t   size;
    size_t   capacity;
    int      used;
};

/* Инициализация + несколько встроенных файлов */
void fs_init(void);

/* Создать пустой файл. 0 = ok, -1 = ошибка */
int  fs_create(const char *name);

/* Удалить файл. 0 = ok, -1 = нет такого */
int  fs_delete(const char *name);

/* Найти файл по имени. NULL если нет */
struct fs_file *fs_find(const char *name);

/* Записать данные (перезаписывает содержимое). 0 = ok */
int  fs_write(const char *name, const void *data, size_t len);

/* Дописать в конец. 0 = ok */
int  fs_append(const char *name, const void *data, size_t len);

/* Прочитать до max байт в buf. Возвращает сколько прочитано, -1 = нет файла */
int  fs_read(const char *name, void *buf, size_t max);

/* Размер файла или (size_t)-1 */
size_t fs_size(const char *name);

/* Список: вызывает cb(name, size, user) для каждого файла */
typedef void (*fs_list_cb)(const char *name, size_t size, void *user);
void fs_list(fs_list_cb cb, void *user);

/* Число файлов */
int  fs_count(void);

#ifdef __cplusplus
}
#endif

#endif
