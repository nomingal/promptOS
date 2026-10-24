#include <stddef.h>

#include "color.h"

struct named_color {
    const char *name;
    uint32_t rgb;
    bool listed;      /* shown in the usage list (aliases are not) */
};

/* Tuned to stay readable on the dark console background. */
static const struct named_color named_colors[] = {
    {"white",   0xFFFFFF, true},
    {"gray",    0xA0A0A0, true},
    {"grey",    0xA0A0A0, false},
    {"red",     0xFF4D4D, true},
    {"orange",  0xFF9F43, true},
    {"yellow",  0xFFD93D, true},
    {"lime",    0xB6F03C, true},
    {"green",   0x3DDC5A, true},
    {"teal",    0x2EC4B6, true},
    {"cyan",    0x2EE6E6, true},
    {"blue",    0x4D8DFF, true},
    {"purple",  0xB07CFF, true},
    {"magenta", 0xFF55FF, true},
    {"pink",    0xFF7EB6, true},
    {"default", COLOR_DEFAULT_RGB, false},
};

#define NAMED_COUNT (sizeof named_colors / sizeof named_colors[0])

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool equals_ignore_case(const char *a, const char *b) {
    while (*a && lower(*a) == lower(*b)) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void split(uint32_t rgb, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (uint8_t)(rgb >> 16);
    *g = (uint8_t)(rgb >> 8);
    *b = (uint8_t)rgb;
}

bool color_parse(const char *text, uint8_t *r, uint8_t *g, uint8_t *b) {
    for (size_t i = 0; i < NAMED_COUNT; i++) {
        if (equals_ignore_case(text, named_colors[i].name)) {
            split(named_colors[i].rgb, r, g, b);
            return true;
        }
    }

    const char *p = (*text == '#') ? text + 1 : text;
    int digit[6];
    size_t n = 0;
    while (p[n]) {
        int v = hex_value(p[n]);
        if (v < 0 || n >= 6) return false;
        digit[n++] = v;
    }

    if (n == 3) {                         /* #rgb -> #rrggbb */
        *r = (uint8_t)(digit[0] * 17);
        *g = (uint8_t)(digit[1] * 17);
        *b = (uint8_t)(digit[2] * 17);
        return true;
    }
    if (n == 6) {
        *r = (uint8_t)(digit[0] * 16 + digit[1]);
        *g = (uint8_t)(digit[2] * 16 + digit[3]);
        *b = (uint8_t)(digit[4] * 16 + digit[5]);
        return true;
    }
    return false;
}

unsigned color_name_count(void) {
    unsigned count = 0;
    for (size_t i = 0; i < NAMED_COUNT; i++) {
        if (named_colors[i].listed) count++;
    }
    return count;
}

const char *color_name(unsigned index) {
    for (size_t i = 0; i < NAMED_COUNT; i++) {
        if (named_colors[i].listed) {
            if (index == 0) return named_colors[i].name;
            index--;
        }
    }
    return 0;
}
