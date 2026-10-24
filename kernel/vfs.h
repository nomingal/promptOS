#ifndef PROMPTOS_VFS_H
#define PROMPTOS_VFS_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_PATH 128
#define MAX_NAME_LEN 32
#define MAX_ITEMS_PER_DIR 16

typedef enum {
    VFS_FILE,
    VFS_DIR
} vfs_type_t;

void vfs_init(void);
const char *vfs_get_cwd(void);
void vfs_autocomplete(const char *input_line, char *buffer, size_t max_len);
bool vfs_ls(const char *path);
bool vfs_cd(const char *path);
bool vfs_mkdir(const char *path);
bool vfs_rmdir(const char *path, bool force);

#endif