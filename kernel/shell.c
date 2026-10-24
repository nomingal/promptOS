#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "console.h"
#include "keyboard.h"
#include "power.h"
#include "shell.h"

#define LINE_MAX 128
#define PROMPT "[promptOS]$ "

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
    console_puts("  help     - show this list\n");
    console_puts("  about    - show information about promptOS\n");
    console_puts("  clear    - clear the screen\n");
    console_puts("  cfg      - configure settings (cfg color | bold | font)\n");
    console_puts("  reboot   - restart the computer\n");
    console_puts("  shutdown - power off the computer\n");
}

static void cmd_about(void) {
    console_puts("promptOS v1.0.0\n");
    console_puts("An OS developed to test an AI's operating system coding skills.\n");
}

static void cmd_color_usage(void) {
    console_puts("usage: cfg color <name | #hex | default>\n");
    console_puts("names:");
    for (unsigned i = 0; i < color_name_count(); i++) {
        console_putc(' ');
        console_puts(color_name(i));
    }
    console_putc('\n');
}

static void cmd_cfg(const char *arg) {
    const char *subarg;
    if ((subarg = args_of(arg, "color")) != NULL) {
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
    } else if ((subarg = args_of(arg, "bold")) != NULL) {
        if (streq(subarg, "on")) {
            console_set_bold(true);
            console_puts("bold enabled\n");
        } else if (streq(subarg, "off")) {
            console_set_bold(false);
            console_puts("bold disabled\n");
        } else {
            console_puts("usage: cfg bold <on | off>\n");
        }
    } else if ((subarg = args_of(arg, "font")) != NULL) {
        if (streq(subarg, "default")) {
            console_set_font(0);
            console_puts("font set to default\n");
        } else if (streq(subarg, "alt")) {
            console_set_font(1);
            console_puts("font set to alternative\n");
        } else {
            console_puts("usage: cfg font <default | alt>\n");
        }
    } else {
        console_puts("usage: cfg <color | bold | font> ...\n");
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
    } else if (streq(line, "reboot")) {
        console_puts("rebooting...\n");
        power_reboot();
    } else if (streq(line, "shutdown")) {
        console_puts("shutting down...\n");
        power_shutdown();
    } else if ((arg = args_of(line, "cfg")) != NULL) {
        cmd_cfg(arg);
    } else {
        console_puts("unknown command: ");
        console_puts(line);
        console_putc('\n');
    }
}

static void read_line(char *buf, size_t max) {
    size_t len = 0;
    for (;;) {
        char c = kbd_getc();
        if (c == '\n') {
            console_putc('\n');
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                console_puts("\b \b");
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
    static char line[LINE_MAX];
    for (;;) {
        console_puts(PROMPT);
        read_line(line, sizeof line);
        shell_exec(line);
    }
}