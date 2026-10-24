#ifndef PROMPTOS_COLOR_H
#define PROMPTOS_COLOR_H

#include <stdbool.h>
#include <stdint.h>

#define COLOR_DEFAULT_RGB 0xE0E0E0u

/* Parse "red", "#ff8800", "ff8800", "#f80" or "default" (case-insensitive). */
bool color_parse(const char *text, uint8_t *r, uint8_t *g, uint8_t *b);

/* Names shown in the color command's usage text. */
unsigned color_name_count(void);
const char *color_name(unsigned index);

#endif
