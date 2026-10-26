/* kernel/console.h */
#ifndef PROMPTOS_CONSOLE_H
#define PROMPTOS_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "boot_info.h"

bool console_init(const struct promptos_framebuffer *fb);
void console_clear(void);
void console_putc(char c);
void console_puts(const char *s);

void console_set_color(uint8_t r, uint8_t g, uint8_t b);
void console_get_color(uint8_t *r, uint8_t *g, uint8_t *b);

void console_set_bgcolor(uint8_t r, uint8_t g, uint8_t b);
void console_get_bgcolor(uint8_t *r, uint8_t *g, uint8_t *b);

void console_set_bold(bool bold);
bool console_get_bold(void);
void console_set_scale(uint32_t scale);
uint32_t console_get_scale(void);
void console_set_font(int font_id);

void console_toggle_cursor(void);
void console_show_cursor(bool show);
void console_draw_line(const char *prompt, const char *buf, size_t cursor_pos);

/* Matrix rain & direct cell drawing helpers */
uint32_t console_get_cols(void);
uint32_t console_get_rows(void);
void console_draw_char(uint32_t col, uint32_t row, char c, uint8_t r, uint8_t g, uint8_t b);

#endif