#ifndef ACORN_FS_H
#define ACORN_FS_H

void fs_init(void);
void fs_sync(void);
int fs_mount(void);
int fs_mkdir(const char *path);
int fs_create(const char *path);
long fs_write(const char *path, const void *data, unsigned long length);
long fs_read(const char *path, void *data, unsigned long capacity);
int fs_exists(const char *path);
int fs_is_directory(const char *path);
long fs_list(const char *directory, char *output, unsigned long capacity);
int fs_remove(const char *path);
int fs_self_test(void);

#endif