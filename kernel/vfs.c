#include "vfs.h"
#include "console.h"

#define MAX_NODES 64

typedef struct {
    char name[MAX_NAME_LEN];
    vfs_type_t type;
    int parent_idx;
    int children[MAX_ITEMS_PER_DIR];
    int child_count;
    bool used;
} vfs_node_t;

static vfs_node_t nodes[MAX_NODES];
static int current_node_idx = 0;
static char cwd_str[MAX_PATH];

static bool streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void str_copy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void str_cat(char *dest, const char *src) {
    while (*dest) {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static size_t path_strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static int create_node(const char *name, vfs_type_t type, int parent) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true;
            str_copy(nodes[i].name, name);
            nodes[i].type = type;
            nodes[i].parent_idx = parent;
            nodes[i].child_count = 0;
            return i;
        }
    }
    return -1;
}

void vfs_init(void) {
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].used = false;
    }

    int root = create_node("/", VFS_DIR, 0);
    nodes[root].parent_idx = 0;

    current_node_idx = root;
    str_copy(cwd_str, "/");
}

const char *vfs_get_cwd(void) {
    return cwd_str;
}

static int resolve_path(const char *path, int *out_parent, char *last_token_out) {
    if (path == NULL || path[0] == '\0') return -1;

    int curr = (path[0] == '/') ? 0 : current_node_idx;
    const char *p = path;
    if (*p == '/') p++;

    char token[MAX_NAME_LEN];
    while (*p != '\0') {
        int t_len = 0;
        while (*p != '\0' && *p != '/') {
            if (t_len < MAX_NAME_LEN - 1) {
                token[t_len++] = *p;
            }
            p++;
        }
        token[t_len] = '\0';

        if (token[0] == '\0') {
            if (*p == '/') p++;
            continue;
        }

        if (streq(token, ".")) {
            // stay
        } else if (streq(token, "..")) {
            curr = nodes[curr].parent_idx;
        } else {
            if (*p == '\0' && out_parent != NULL && last_token_out != NULL) {
                *out_parent = curr;
                str_copy(last_token_out, token);
            }

            bool found = false;
            vfs_node_t *d = &nodes[curr];
            for (int i = 0; i < d->child_count; i++) {
                int child_idx = d->children[i];
                if (streq(nodes[child_idx].name, token)) {
                    curr = child_idx;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return -1;
            }
        }

        if (*p == '/') p++;
    }

    if (*path != '\0' && path[0] != '\0' && path[path_strlen(path) - 1] == '/') {
        if (out_parent != NULL && last_token_out != NULL) {
            *out_parent = curr;
            last_token_out[0] = '\0';
        }
    }

    return curr;
}

void vfs_autocomplete(const char *input_line, char *buffer, size_t max_len) {
    buffer[0] = '\0';
    if (input_line == NULL) return;

    // Find the last slash to separate parent path from the prefix token
    const char *last_slash = NULL;
    for (const char *p = input_line; *p != '\0'; p++) {
        if (*p == '/') last_slash = p;
    }

    int target_dir = current_node_idx;
    const char *prefix = input_line;

    if (last_slash != NULL) {
        char parent_path[MAX_PATH];
        size_t len = last_slash - input_line;
        if (len == 0) {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        } else {
            for (size_t i = 0; i < len; i++) parent_path[i] = input_line[i];
            parent_path[len] = '\0';
        }

        target_dir = resolve_path(parent_path, NULL, NULL);
        if (target_dir == -1 || nodes[target_dir].type != VFS_DIR) {
            return;
        }
        prefix = last_slash + 1;
    }

    size_t prefix_len = path_strlen(prefix);
    vfs_node_t *dir = &nodes[target_dir];

    const char *matches[MAX_ITEMS_PER_DIR];
    int match_count = 0;

    for (int i = 0; i < dir->child_count; i++) {
        const char *name = nodes[dir->children[i]].name;
        bool match = true;
        for (size_t j = 0; j < prefix_len; j++) {
            if (name[j] != prefix[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            matches[match_count++] = name;
        }
    }

    if (match_count == 0) return;

    size_t common_extra = 0;
    for (;;) {
        if (prefix_len + common_extra >= MAX_NAME_LEN - 1) break;
        char c = matches[0][prefix_len + common_extra];
        if (c == '\0') break;

        bool all_match = true;
        for (int m = 1; m < match_count; m++) {
            if (matches[m][prefix_len + common_extra] != c) {
                all_match = false;
                break;
            }
        }
        if (!all_match) break;
        common_extra++;
    }

    if (common_extra > 0 && common_extra < max_len) {
        for (size_t i = 0; i < common_extra; i++) {
            buffer[i] = matches[0][prefix_len + i];
        }
        buffer[common_extra] = '\0';
    }
}

bool vfs_ls(const char *path) {
    int target_idx = current_node_idx;
    if (path != NULL && path[0] != '\0') {
        target_idx = resolve_path(path, NULL, NULL);
        if (target_idx == -1) {
            console_puts("ls: no such file or directory: ");
            console_puts(path);
            console_putc('\n');
            return false;
        }
    }

    vfs_node_t *dir = &nodes[target_idx];
    if (dir->type != VFS_DIR) {
        console_puts(dir->name);
        console_putc('\n');
        return true;
    }

    for (int i = 0; i < dir->child_count; i++) {
        int child_idx = dir->children[i];
        console_puts(nodes[child_idx].name);
        if (nodes[child_idx].type == VFS_DIR) {
            console_puts("/");
        }
        console_puts("  ");
    }
    console_putc('\n');
    return true;
}

static void rebuild_cwd(void) {
    if (current_node_idx == 0) {
        str_copy(cwd_str, "/");
        return;
    }

    int path_indices[16];
    int depth = 0;
    int curr = current_node_idx;

    while (curr != 0 && depth < 16) {
        path_indices[depth++] = curr;
        curr = nodes[curr].parent_idx;
    }

    cwd_str[0] = '\0';
    for (int i = depth - 1; i >= 0; i--) {
        str_cat(cwd_str, "/");
        str_cat(cwd_str, nodes[path_indices[i]].name);
    }
}

bool vfs_cd(const char *path) {
    if (path == NULL || streq(path, "")) {
        return true;
    }

    int target = resolve_path(path, NULL, NULL);
    if (target == -1) {
        console_puts("no such directory: ");
        console_puts(path);
        console_putc('\n');
        return false;
    }

    if (nodes[target].type != VFS_DIR) {
        console_puts("not a directory: ");
        console_puts(path);
        console_putc('\n');
        return false;
    }

    current_node_idx = target;
    rebuild_cwd();
    return true;
}

bool vfs_mkdir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        console_puts("usage: mkdir <path>\n");
        return false;
    }

    int parent_idx = current_node_idx;
    char dirname[MAX_NAME_LEN];

    const char *last_slash = NULL;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/') last_slash = p;
    }

    if (last_slash != NULL) {
        char parent_path[MAX_PATH];
        size_t parent_len = last_slash - path;
        if (parent_len == 0) {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        } else {
            for (size_t i = 0; i < parent_len; i++) parent_path[i] = path[i];
            parent_path[parent_len] = '\0';
        }

        parent_idx = resolve_path(parent_path, NULL, NULL);
        if (parent_idx == -1 || nodes[parent_idx].type != VFS_DIR) {
            console_puts("mkdir: invalid path or parent directory not found\n");
            return false;
        }
        str_copy(dirname, last_slash + 1);
    } else {
        str_copy(dirname, path);
    }

    if (dirname[0] == '\0') {
        console_puts("mkdir: missing directory name\n");
        return false;
    }

    if (streq(dirname, ".") || streq(dirname, "..")) {
        console_puts("mkdir: permission denied\n");
        return false;
    }

    vfs_node_t *dir = &nodes[parent_idx];
    for (int i = 0; i < dir->child_count; i++) {
        if (streq(nodes[dir->children[i]].name, dirname)) {
            console_puts("mkdir: directory exists: ");
            console_puts(dirname);
            console_putc('\n');
            return false;
        }
    }

    if (dir->child_count >= MAX_ITEMS_PER_DIR) {
        console_puts("mkdir: directory full\n");
        return false;
    }

    int new_dir_idx = create_node(dirname, VFS_DIR, parent_idx);
    if (new_dir_idx == -1) {
        console_puts("mkdir: max system nodes reached\n");
        return false;
    }

    dir->children[dir->child_count++] = new_dir_idx;
    return true;
}

static void recursive_free_node(int node_idx) {
    vfs_node_t *node = &nodes[node_idx];
    for (int i = 0; i < node->child_count; i++) {
        recursive_free_node(node->children[i]);
    }
    node->used = false;
    node->child_count = 0;
}

bool vfs_rmdir(const char *path, bool force) {
    if (path == NULL || path[0] == '\0') {
        console_puts("usage: rmdir [-f] <path>\n");
        return false;
    }

    int parent_idx = current_node_idx;
    char dirname[MAX_NAME_LEN];

    const char *last_slash = NULL;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/') last_slash = p;
    }

    if (last_slash != NULL) {
        char parent_path[MAX_PATH];
        size_t parent_len = last_slash - path;
        if (parent_len == 0) {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        } else {
            for (size_t i = 0; i < parent_len; i++) parent_path[i] = path[i];
            parent_path[parent_len] = '\0';
        }

        parent_idx = resolve_path(parent_path, NULL, NULL);
        if (parent_idx == -1 || nodes[parent_idx].type != VFS_DIR) {
            console_puts("rmdir: parent directory not found\n");
            return false;
        }
        str_copy(dirname, last_slash + 1);
    } else {
        str_copy(dirname, path);
    }

    vfs_node_t *dir = &nodes[parent_idx];
    int target_i = -1;
    int target_idx = -1;

    for (int i = 0; i < dir->child_count; i++) {
        if (streq(nodes[dir->children[i]].name, dirname)) {
            target_i = i;
            target_idx = dir->children[i];
            break;
        }
    }

    if (target_idx == -1) {
        console_puts("rmdir: no such directory\n");
        return false;
    }

    if (nodes[target_idx].child_count > 0 && !force) {
        console_puts("rmdir: directory not empty (use -f)\n");
        return false;
    }

    if (force) {
        recursive_free_node(target_idx);
    } else {
        nodes[target_idx].used = false;
    }

    for (int i = target_i; i < dir->child_count - 1; i++) {
        dir->children[i] = dir->children[i + 1];
    }
    dir->child_count--;

    return true;
}