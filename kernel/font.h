#ifndef PROMPTOS_FONT_H
#define PROMPTOS_FONT_H

#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* ASCII 32..126, one byte per row, most significant bit = leftmost pixel. */
extern const uint8_t font8x16[95][16];
extern const uint8_t font8x16_alt[95][16];

#endif