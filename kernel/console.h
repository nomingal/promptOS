#ifndef PROMPTOS_CONSOLE_H
#define PROMPTOS_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>
#include "limine_proto.h"

bool console_init(const struct limine_framebuffer *fb);

void console_putc(char c);
void console_puts(const char *s);
void console_clear(void);

void console_set_color(uint8_t r, uint8_t g, uint8_t b);
void console_set_bold(bool bold);
void console_set_font(int font_id);

#endif