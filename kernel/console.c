/* kernel/console.c */
#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "console.h"
#include "font.h"
#include "boot_info.h"

#define MAX_COLS 256
#define MAX_ROWS 128

static uint8_t *fb_base;
static uint64_t fb_pitch;
static uint32_t fb_width, fb_height;

static uint32_t scale = 1;
static uint32_t cols, rows;
static uint32_t margin_x, margin_y;
static uint32_t cur_col, cur_row;
static uint32_t fg_pixel, bg_pixel;

static uint8_t current_fg_r = 0xFF, current_fg_g = 0xFF, current_fg_b = 0xFF;
static uint8_t current_bg_r = 0x10, current_bg_g = 0x12, current_bg_b = 0x18;

static struct promptos_framebuffer fbinfo;
static bool ready;
static bool bold_enabled = false;
static int current_font = 0;
static bool cursor_state = true;

static char cells[MAX_ROWS][MAX_COLS];

static uint32_t pack(const struct promptos_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r >> (8 - fb->red_mask_size)) << fb->red_mask_shift) |
           ((uint32_t)(g >> (8 - fb->green_mask_size)) << fb->green_mask_shift) |
           ((uint32_t)(b >> (8 - fb->blue_mask_size)) << fb->blue_mask_shift);
}

static void fill_screen(uint32_t pixel) {
    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t *line = (uint32_t *)(fb_base + (uint64_t)y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++) {
            line[x] = pixel;
        }
    }
}

static void update_metrics(void) {
    uint32_t pad = FONT_WIDTH * scale;
    uint32_t cell_w = FONT_WIDTH * scale;
    uint32_t cell_h = FONT_HEIGHT * scale;
    cols = (fb_width > 2 * pad) ? (fb_width - 2 * pad) / cell_w : 0;
    rows = (fb_height > 2 * pad) ? (fb_height - 2 * pad) / cell_h : 0;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (cols == 0) cols = 1;
    if (rows == 0) rows = 1;
    margin_x = (fb_width - cols * cell_w) / 2;
    margin_y = (fb_height - rows * cell_h) / 2;
    if (cur_col >= cols) cur_col = cols - 1;
    if (cur_row >= rows) cur_row = rows - 1;
}

static void draw_cell(uint32_t col, uint32_t row, bool inverted) {
    char ch = cells[row][col];
    size_t idx = (ch >= 32 && ch <= 126) ? ch - 32 : 0;
    const uint8_t *glyph = font8x16[idx];

    uint32_t fg = inverted ? bg_pixel : fg_pixel;
    uint32_t bg = inverted ? fg_pixel : bg_pixel;
    uint32_t x0 = margin_x + col * FONT_WIDTH * scale;
    uint32_t y0 = margin_y + row * FONT_HEIGHT * scale;

    for (uint32_t gy = 0; gy < FONT_HEIGHT; gy++) {
        uint8_t bits = glyph[gy];
        if (bold_enabled) {
            bits |= (bits >> 1);
        }
        for (uint32_t sy = 0; sy < scale; sy++) {
            uint32_t *p = (uint32_t *)(fb_base + (uint64_t)(y0 + gy * scale + sy) * fb_pitch) + x0;
            for (uint32_t gx = 0; gx < FONT_WIDTH; gx++) {
                uint32_t color = (bits & (0x80u >> gx)) ? fg : bg;
                for (uint32_t sx = 0; sx < scale; sx++) {
                    *p++ = color;
                }
            }
        }
    }
}

static void redraw_all(void) {
    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            draw_cell(c, r, false);
        }
    }
}

static void newline(void) {
    cur_col = 0;
    if (cur_row + 1 < rows) {
        cur_row++;
        return;
    }
    for (uint32_t r = 1; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            cells[r - 1][c] = cells[r][c];
        }
    }
    for (uint32_t c = 0; c < cols; c++) {
        cells[rows - 1][c] = 0;
    }
    redraw_all();
}

bool console_init(const struct promptos_framebuffer *fb) {
    if (fb == 0 || fb->address == 0 || fb->bpp != 32 ||
        fb->memory_model != PROMPTOS_FRAMEBUFFER_RGB) {
        return false;
    }

    fbinfo = *fb;
    fb_base = fb->address;
    fb_pitch = fb->pitch;
    fb_width = (uint32_t)fb->width;
    fb_height = (uint32_t)fb->height;

    scale = 1;
    update_metrics();

    current_fg_r = (COLOR_DEFAULT_RGB >> 16) & 0xFF;
    current_fg_g = (COLOR_DEFAULT_RGB >> 8) & 0xFF;
    current_fg_b = COLOR_DEFAULT_RGB & 0xFF;
    current_bg_r = 0x10;
    current_bg_g = 0x12;
    current_bg_b = 0x18;

    fg_pixel = pack(fb, current_fg_r, current_fg_g, current_fg_b);
    bg_pixel = pack(fb, current_bg_r, current_bg_g, current_bg_b);

    ready = true;
    console_clear();
    return true;
}

void console_clear(void) {
    if (!ready) return;
    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            cells[r][c] = 0;
        }
    }
    cur_col = 0;
    cur_row = 0;
    fill_screen(bg_pixel);
    cursor_state = true;
    draw_cell(cur_col, cur_row, true);
}

void console_toggle_cursor(void) {
    if (!ready) return;
    cursor_state = !cursor_state;
    draw_cell(cur_col, cur_row, cursor_state);
}

void console_show_cursor(bool show) {
    if (!ready) return;
    cursor_state = show;
    draw_cell(cur_col, cur_row, cursor_state);
}

void console_draw_line(const char *prompt, const char *buf, size_t cursor_pos) {
    if (!ready) return;

    for (uint32_t c = 0; c < cols; c++) {
        cells[cur_row][c] = 0;
    }

    uint32_t cell_h = FONT_HEIGHT * scale;
    uint32_t y0 = margin_y + cur_row * cell_h;
    for (uint32_t y = 0; y < cell_h; y++) {
        uint32_t *line = (uint32_t *)(fb_base + (uint64_t)(y0 + y) * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++) {
            line[x] = bg_pixel;
        }
    }

    cur_col = 0;

    const char *p = prompt;
    while (*p && cur_col < cols) {
        cells[cur_row][cur_col] = *p;
        draw_cell(cur_col, cur_row, false);
        cur_col++;
        p++;
    }

    uint32_t prompt_cols = cur_col;

    const char *b = buf;
    while (*b && cur_col < cols) {
        cells[cur_row][cur_col] = *b;
        draw_cell(cur_col, cur_row, false);
        cur_col++;
        b++;
    }

    cur_col = prompt_cols + (uint32_t)cursor_pos;
    if (cur_col >= cols) cur_col = cols - 1;

    cursor_state = true;
    draw_cell(cur_col, cur_row, true);
}

void console_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!ready) return;
    current_fg_r = r;
    current_fg_g = g;
    current_fg_b = b;
    fg_pixel = pack(&fbinfo, r, g, b);
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_state);
}

void console_get_color(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (r) *r = current_fg_r;
    if (g) *g = current_fg_g;
    if (b) *b = current_fg_b;
}

void console_set_bgcolor(uint8_t r, uint8_t g, uint8_t b) {
    if (!ready) return;
    current_bg_r = r;
    current_bg_g = g;
    current_bg_b = b;
    bg_pixel = pack(&fbinfo, r, g, b);
    fill_screen(bg_pixel);
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_state);
}

void console_get_bgcolor(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (r) *r = current_bg_r;
    if (g) *g = current_bg_g;
    if (b) *b = current_bg_b;
}

void console_set_bold(bool bold) {
    if (!ready || bold_enabled == bold) return;
    bold_enabled = bold;
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_state);
}

bool console_get_bold(void) {
    return bold_enabled;
}

void console_set_scale(uint32_t new_scale) {
    if (!ready || new_scale < 1 || new_scale > 4 || scale == new_scale) return;
    scale = new_scale;
    update_metrics();
    fill_screen(bg_pixel);
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_state);
}

uint32_t console_get_scale(void) {
    return scale;
}

void console_set_font(int font_id) {
    if (!ready || current_font == font_id) return;
    current_font = font_id;
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_state);
}

void console_putc(char c) {
    if (!ready) return;

    draw_cell(cur_col, cur_row, false);

    if (c == '\n') {
        newline();
    } else if (c == '\r') {
        cur_col = 0;
    } else if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
        } else if (cur_row > 0) {
            cur_row--;
            cur_col = cols - 1;
        }
    } else if (c >= 32 && c <= 126) {
        cells[cur_row][cur_col] = c;
        draw_cell(cur_col, cur_row, false);
        if (++cur_col >= cols) {
            newline();
        }
    }

    draw_cell(cur_col, cur_row, cursor_state);
}

void console_puts(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}

uint32_t console_get_cols(void) {
    return cols;
}

uint32_t console_get_rows(void) {
    return rows;
}

void console_draw_char(uint32_t col, uint32_t row, char c, uint8_t r, uint8_t g, uint8_t b) {
    if (!ready || col >= cols || row >= rows) return;

    cells[row][col] = c;
    size_t idx = (c >= 32 && c <= 126) ? c - 32 : 0;
    const uint8_t *glyph = font8x16[idx];

    uint32_t fg = pack(&fbinfo, r, g, b);
    uint32_t bg = bg_pixel;
    uint32_t x0 = margin_x + col * FONT_WIDTH * scale;
    uint32_t y0 = margin_y + row * FONT_HEIGHT * scale;

    for (uint32_t gy = 0; gy < FONT_HEIGHT; gy++) {
        uint8_t bits = glyph[gy];
        if (bold_enabled) {
            bits |= (bits >> 1);
        }
        for (uint32_t sy = 0; sy < scale; sy++) {
            uint32_t *p = (uint32_t *)(fb_base + (uint64_t)(y0 + gy * scale + sy) * fb_pitch) + x0;
            for (uint32_t gx = 0; gx < FONT_WIDTH; gx++) {
                uint32_t color = (bits & (0x80u >> gx)) ? fg : bg;
                for (uint32_t sx = 0; sx < scale; sx++) {
                    *p++ = color;
                }
            }
        }
    }
}