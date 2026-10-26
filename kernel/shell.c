/* kernel/shell.c */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "console.h"
#include "keyboard.h"
#include "power.h"
#include "rain.h"
#include "shell.h"
#include "vfs.h"

#define LINE_MAX 128
#define HISTORY_MAX 16

static char history[HISTORY_MAX][LINE_MAX];
static int history_count = 0;

static bool streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static char *trim(char *s) {
    while (*s == ' ') s++;
    char *end = s;
    while (*end) end++;
    while (end > s && end[-1] == ' ') *--end = '\0';
    return s;
}

static const char *args_of(const char *line, const char *cmd) {
    while (*cmd) {
        if (*line++ != *cmd++) return NULL;
    }
    if (*line == '\0') return line;
    if (*line != ' ') return NULL;
    while (*line == ' ') line++;
    return line;
}

static void cmd_help(void) {
    console_puts("commands:\n");
    console_puts("  help            - show this list\n");
    console_puts("  about           - show information about promptOS\n");
    console_puts("  clear           - clear the screen\n");
    console_puts("  ls [dir]        - list directory contents\n");
    console_puts("  cd <dir>        - change directory\n");
    console_puts("  mkdir <dir>     - create a new directory\n");
    console_puts("  rmdir [-f]      - remove a directory (use -f to force)\n");
    console_puts("  cfg             - configure settings (cfg font ..., cfg bg ...)\n");
    console_puts("  rain [speed 1-5]- matrix rain (S key changes speed during rain)\n");
    console_puts("  reboot          - restart the computer\n");
    console_puts("  shutdown        - power off the computer\n");
}

static void cmd_about(void) {
    console_puts("promptOS v1.2.0\n");
    console_puts("An OS developed to test an AI's operating system coding skills.\n");
}

static void cmd_color_usage(void) {
    console_puts("usage: cfg font color <name | #hex>\n");
    console_puts("names:");
    for (unsigned i = 0; i < color_name_count(); i++) {
        console_putc(' ');
        console_puts(color_name(i));
    }
    console_putc('\n');
}

static void cmd_bgcolor_usage(void) {
    console_puts("usage: cfg bg color <name | #hex>\n");
    console_puts("names:");
    for (unsigned i = 0; i < color_name_count(); i++) {
        console_putc(' ');
        console_puts(color_name(i));
    }
    console_putc('\n');
}

static void cmd_font_cfg(const char *arg) {
    const char *subarg;

    if (streq(arg, "bold")) {
        bool current = console_get_bold();
        console_set_bold(!current);
        console_puts(console_get_bold() ? "bold enabled\n" : "bold disabled\n");
    } else if ((subarg = args_of(arg, "bold")) != NULL) {
        if (streq(subarg, "on")) {
            console_set_bold(true);
            console_puts("bold enabled\n");
        } else if (streq(subarg, "off")) {
            console_set_bold(false);
            console_puts("bold disabled\n");
        } else {
            console_puts("usage: cfg font bold <on | off>\n");
        }
    } else if ((subarg = args_of(arg, "size")) != NULL) {
        if (*subarg >= '1' && *subarg <= '4' && subarg[1] == '\0') {
            uint32_t new_scale = (uint32_t)(*subarg - '0');
            console_set_scale(new_scale);
            console_puts("font size updated\n");
        } else {
            console_puts("usage: cfg font size <1-4>\n");
        }
    } else if ((subarg = args_of(arg, "color")) != NULL) {
        if (*subarg == '\0') {
            cmd_color_usage();
            return;
        }
        uint8_t r, g, b;
        if (!color_parse(subarg, &r, &g, &b)) {
            console_puts("unknown color: ");
            console_puts(subarg);
            console_puts("\n");
            return;
        }

        uint8_t bg_r, bg_g, bg_b;
        console_get_bgcolor(&bg_r, &bg_g, &bg_b);
        if (r == bg_r && g == bg_g && b == bg_b) {
            console_puts("font color cannot be the same as background color\n");
            return;
        }

        console_set_color(r, g, b);
        console_puts("font color updated\n");
    } else {
        console_puts("usage: cfg font <color | bold | size>\n");
    }
}

static void cmd_bg_cfg(const char *arg) {
    const char *subarg;
    const char *color_str = NULL;

    if ((subarg = args_of(arg, "color")) != NULL) {
        if (*subarg == '\0') {
            cmd_bgcolor_usage();
            return;
        }
        color_str = subarg;
    } else if (*arg != '\0') {
        color_str = arg;
    } else {
        console_puts("usage: cfg bg color <name | #hex>\n");
        return;
    }

    uint8_t r, g, b;
    if (!color_parse(color_str, &r, &g, &b)) {
        console_puts("unknown color: ");
        console_puts(color_str);
        console_puts("\n");
        return;
    }

    uint8_t fg_r, fg_g, fg_b;
    console_get_color(&fg_r, &fg_g, &fg_b);
    if (r == fg_r && g == fg_g && b == fg_b) {
        console_puts("background color cannot be the same as font color\n");
        return;
    }

    console_set_bgcolor(r, g, b);
    console_puts("background color updated\n");
}

static void cmd_cfg(const char *arg) {
    const char *subarg;
    if ((subarg = args_of(arg, "font")) != NULL) {
        if (*subarg == '\0') {
            console_puts("usage: cfg font <color | bold | size>\n");
        } else {
            cmd_font_cfg(subarg);
        }
    } else if ((subarg = args_of(arg, "bg")) != NULL) {
        cmd_bg_cfg(subarg);
    } else {
        console_puts("usage: cfg <font | bg> ...\n");
    }
}

void shell_exec(char *line) {
    line = trim(line);
    if (*line == '\0') {
        return;
    }

    const char *arg;
    if (streq(line, "help")) {
        cmd_help();
    } else if (streq(line, "about")) {
        cmd_about();
    } else if (streq(line, "clear")) {
        console_clear();
    } else if (streq(line, "ls")) {
        vfs_ls(NULL);
    } else if ((arg = args_of(line, "ls")) != NULL) {
        vfs_ls(arg);
    } else if ((arg = args_of(line, "cd")) != NULL) {
        vfs_cd(arg);
    } else if ((arg = args_of(line, "mkdir")) != NULL) {
        vfs_mkdir(arg);
    } else if ((arg = args_of(line, "rmdir")) != NULL) {
        const char *target = arg;
        bool force = false;
        if (arg[0] == '-' && arg[1] == 'f' && (arg[2] == ' ' || arg[2] == '\0')) {
            force = true;
            target = arg + 2;
            while (*target == ' ') target++;
        }
        vfs_rmdir(target, force);
    } else if (streq(line, "rain")) {
        rain_run(2); /* Slower default speed (level 2) */
    } else if ((arg = args_of(line, "rain")) != NULL) {
        int spd = 2;
        const char *s = arg;
        const char *subarg;
        if ((subarg = args_of(arg, "speed")) != NULL) {
            s = subarg;
        }
        if (*s >= '1' && *s <= '5') {
            spd = *s - '0';
        }
        rain_run(spd);
    } else if (streq(line, "reboot")) {
        console_puts("rebooting...\n");
        power_reboot();
    } else if (streq(line, "shutdown")) {
        console_puts("shutting down...\n");
        power_shutdown();
    } else if (streq(line, "cfg")) {
        cmd_cfg("");
    } else if ((arg = args_of(line, "cfg")) != NULL) {
        cmd_cfg(arg);
    } else {
        console_puts("unknown command: ");
        console_puts(line);
        console_putc('\n');
    }
}

static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void build_prompt(char *p, size_t max) {
    const char *prefix = "[promptOS:";
    const char *cwd = vfs_get_cwd();
    const char *suffix = "]$ ";
    size_t i = 0;
    while (*prefix && i < max - 1) p[i++] = *prefix++;
    while (*cwd && i < max - 1) p[i++] = *cwd++;
    while (*suffix && i < max - 1) p[i++] = *suffix++;
    p[i] = '\0';
}

static void read_line(char *buf, size_t max, const char *prompt) {
    size_t len = 0;
    size_t cursor = 0;

    int hist_idx = history_count;
    char current_edit[LINE_MAX] = {0};

    console_draw_line(prompt, buf, cursor);

    uint64_t last_blink = read_tsc();
    uint64_t blink_threshold = 1000000000ULL;

    for (;;) {
        uint64_t now = read_tsc();
        if (now - last_blink >= blink_threshold) {
            console_toggle_cursor();
            last_blink = now;
        }

        char c = kbd_getc();
        if (c == 0) {
            for (volatile int delay = 0; delay < 100000; delay++);
            continue;
        }

        console_show_cursor(true);
        last_blink = read_tsc();

        if (c == '\n') {
            console_show_cursor(false);
            console_putc('\n');
            break;
        }
        
        if (c == '\b') {
            if (cursor > 0) {
                for (size_t i = cursor - 1; i < len - 1; i++) buf[i] = buf[i + 1];
                len--;
                cursor--;
                buf[len] = '\0';
                console_draw_line(prompt, buf, cursor);
            }
            continue;
        }

        if (c == '\x14') { /* Left Arrow */
            if (cursor > 0) {
                cursor--;
                console_draw_line(prompt, buf, cursor);
            }
            continue;
        }

        if (c == '\x13') { /* Right Arrow */
            if (cursor < len) {
                cursor++;
                console_draw_line(prompt, buf, cursor);
            }
            continue;
        }

        if (c == '\x11') { /* Up Arrow */
            if (hist_idx > 0) {
                if (hist_idx == history_count) {
                    for (size_t i = 0; i <= len; i++) current_edit[i] = buf[i];
                }
                hist_idx--;
                len = 0;
                while (history[hist_idx][len] && len < max - 1) {
                    buf[len] = history[hist_idx][len];
                    len++;
                }
                buf[len] = '\0';
                cursor = len;
                console_draw_line(prompt, buf, cursor);
            }
            continue;
        }

        if (c == '\x12') { /* Down Arrow */
            if (hist_idx < history_count) {
                hist_idx++;
                len = 0;
                if (hist_idx == history_count) {
                    while (current_edit[len] && len < max - 1) {
                        buf[len] = current_edit[len];
                        len++;
                    }
                } else {
                    while (history[hist_idx][len] && len < max - 1) {
                        buf[len] = history[hist_idx][len];
                        len++;
                    }
                }
                buf[len] = '\0';
                cursor = len;
                console_draw_line(prompt, buf, cursor);
            }
            continue;
        }

        if (c == '\t') {
            int start = (int)cursor;
            while (start > 0 && buf[start - 1] != ' ') start--;
            
            char prefix[64];
            size_t p_len = cursor - start;
            if (p_len < sizeof(prefix)) {
                for (size_t i = 0; i < p_len; i++) prefix[i] = buf[start + i];
                prefix[p_len] = '\0';

                char completion[32];
                vfs_autocomplete(prefix, completion, sizeof(completion));
                
                char *comp_ptr = completion;
                while (*comp_ptr && len < max - 1) {
                    for (size_t i = len; i > cursor; i--) {
                        buf[i] = buf[i - 1];
                    }
                    buf[cursor] = *comp_ptr++;
                    len++;
                    cursor++;
                    buf[len] = '\0';
                }
                console_draw_line(prompt, buf, cursor);
            }
            continue;
        }

        if (c >= 32 && c < 127 && len < max - 1) {
            for (size_t i = len; i > cursor; i--) {
                buf[i] = buf[i - 1];
            }
            buf[cursor] = c;
            len++;
            cursor++;
            buf[len] = '\0';
            console_draw_line(prompt, buf, cursor);
        }
    }
    buf[len] = '\0';
}

void shell_run(void) {
    vfs_init();
    static char line[LINE_MAX];
    char prompt[128];
    for (;;) {
        build_prompt(prompt, sizeof(prompt));
        line[0] = '\0';
        read_line(line, sizeof(line), prompt);
        
        if (line[0] != '\0') {
            if (history_count == 0 || !streq(history[history_count - 1], line)) {
                if (history_count < HISTORY_MAX) {
                    for (size_t i = 0; i < LINE_MAX; i++) {
                        history[history_count][i] = line[i];
                        if (line[i] == '\0') break;
                    }
                    history_count++;
                } else {
                    for (int i = 1; i < HISTORY_MAX; i++) {
                        for (size_t j = 0; j < LINE_MAX; j++) {
                            history[i - 1][j] = history[i][j];
                        }
                    }
                    for (size_t i = 0; i < LINE_MAX; i++) {
                        history[HISTORY_MAX - 1][i] = line[i];
                        if (line[i] == '\0') break;
                    }
                }
            }
        }
        shell_exec(line);
    }
}