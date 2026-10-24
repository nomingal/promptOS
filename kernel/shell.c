#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "console.h"
#include "keyboard.h"
#include "power.h"
#include "shell.h"
#include "vfs.h"

#define LINE_MAX 128

static bool streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static char *trim(char *s) {
    while (*s == ' ') {
        s++;
    }
    char *end = s;
    while (*end) {
        end++;
    }
    while (end > s && end[-1] == ' ') {
        *--end = '\0';
    }
    return s;
}

static const char *args_of(const char *line, const char *cmd) {
    while (*cmd) {
        if (*line++ != *cmd++) {
            return NULL;
        }
    }
    if (*line == '\0') {
        return line;
    }
    if (*line != ' ') {
        return NULL;
    }
    while (*line == ' ') {
        line++;
    }
    return line;
}

static void cmd_help(void) {
    console_puts("commands:\n");
    console_puts("  help        - show this list\n");
    console_puts("  about       - show information about promptOS\n");
    console_puts("  clear       - clear the screen\n");
    console_puts("  ls [dir]    - list directory contents\n");
    console_puts("  cd <dir>    - change directory\n");
    console_puts("  mkdir <dir> - create a new directory\n");
    console_puts("  rmdir [-f]  - remove a directory (use -f to force)\n");
    console_puts("  cfg         - configure settings (cfg font ...)\n");
    console_puts("  reboot      - restart the computer\n");
    console_puts("  shutdown    - power off the computer\n");
}

static void cmd_about(void) {
    console_puts("promptOS v1.1.0\n");
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
        if ((r * 299u + g * 587u + b * 114u) / 1000u < 48u) {
            console_puts("that color is too dark to read\n");
            return;
        }
        console_set_color(r, g, b);
        console_puts("font color updated\n");
    } else {
        console_puts("usage: cfg font <color | bold | size>\n");
    }
}

static void cmd_cfg(const char *arg) {
    const char *subarg;
    if ((subarg = args_of(arg, "font")) != NULL) {
        if (*subarg == '\0') {
            console_puts("usage: cfg font <color | bold | size>\n");
        } else {
            cmd_font_cfg(subarg);
        }
    } else {
        console_puts("usage: cfg <font> ...\n");
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

static void read_line(char *buf, size_t max) {
    size_t len = 0;

    console_show_cursor(true);

    uint64_t last_blink = read_tsc();
    /* Calibrated for ~0.5s cursor toggling on modern CPUs */
    uint64_t blink_threshold = 1000000000ULL;

    for (;;) {
        uint64_t now = read_tsc();
        if (now - last_blink >= blink_threshold) {
            console_toggle_cursor();
            last_blink = now;
        }

        char c = kbd_getc();
        if (c == 0) {
            /* Spin delay to prevent SMM interrupt gridlock from high-frequency inb(0x64) calls */
            for (volatile int delay = 0; delay < 100000; delay++);
            continue;
        }

        /* On key press: force solid visible cursor and reset timer */
        console_show_cursor(true);
        last_blink = read_tsc();

        if (c == '\n') {
            console_putc('\n');
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                console_putc('\b');
            }
            continue;
        }
        if (c == '\t') {
            int start = (int)len;
            while (start > 0 && buf[start - 1] != ' ') {
                start--;
            }
            char prefix[64];
            size_t p_len = len - start;
            if (p_len < sizeof(prefix)) {
                for (size_t i = 0; i < p_len; i++) {
                    prefix[i] = buf[start + i];
                }
                prefix[p_len] = '\0';

                char completion[32];
                vfs_autocomplete(prefix, completion, sizeof(completion));
                
                char *comp_ptr = completion;
                while (*comp_ptr && len < max - 1) {
                    buf[len++] = *comp_ptr;
                    console_putc(*comp_ptr);
                    comp_ptr++;
                }
            }
            continue;
        }
        if (c >= 32 && c < 127 && len < max - 1) {
            buf[len++] = c;
            console_putc(c);
        }
    }
    buf[len] = '\0';
}

void shell_run(void) {
    vfs_init();
    static char line[LINE_MAX];
    for (;;) {
        console_puts("[promptOS:");
        console_puts(vfs_get_cwd());
        console_puts("]$ ");
        read_line(line, sizeof line);
        shell_exec(line);
    }
}