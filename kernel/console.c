#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "console.h"
#include "font.h"

#define MAX_COLS 256
#define MAX_ROWS 128

static uint8_t *fb_base;
static uint64_t fb_pitch;
static uint32_t fb_width, fb_height;

static uint32_t scale;
static uint32_t cols, rows;
static uint32_t margin_x, margin_y;
static uint32_t cur_col, cur_row;
static uint32_t fg_pixel, bg_pixel;
static struct limine_framebuffer fbinfo;
static bool ready;
static bool bold_enabled = false;
static int current_font = 0;             /* 0 = default, 1 = alt */

static char cells[MAX_ROWS][MAX_COLS];

static uint32_t pack(const struct limine_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b) {
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

static void draw_cell(uint32_t col, uint32_t row, bool inverted) {
    char ch = cells[row][col];
    size_t idx = (ch >= 32 && ch <= 126) ? ch - 32 : 0;
    const uint8_t *glyph = (current_font == 1) ? font8x16_alt[idx] : font8x16[idx];

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

bool console_init(const struct limine_framebuffer *fb) {
    if (fb == 0 || fb->address == 0 || fb->bpp != 32 ||
        fb->memory_model != LIMINE_FRAMEBUFFER_RGB) {
        return false;
    }

    fbinfo = *fb;
    fb_base = fb->address;
    fb_pitch = fb->pitch;
    fb_width = (uint32_t)fb->width;
    fb_height = (uint32_t)fb->height;

    scale = fb_height / 480;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    while (scale > 1 && fb_width < 80 * FONT_WIDTH * scale) {
        scale--;
    }

    uint32_t pad = FONT_WIDTH * scale;
    uint32_t cell_w = FONT_WIDTH * scale;
    uint32_t cell_h = FONT_HEIGHT * scale;
    cols = (fb_width > 2 * pad) ? (fb_width - 2 * pad) / cell_w : 0;
    rows = (fb_height > 2 * pad) ? (fb_height - 2 * pad) / cell_h : 0;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (cols == 0 || rows == 0) {
        return false;
    }
    margin_x = (fb_width - cols * cell_w) / 2;
    margin_y = (fb_height - rows * cell_h) / 2;

    fg_pixel = pack(fb, (COLOR_DEFAULT_RGB >> 16) & 0xFF, (COLOR_DEFAULT_RGB >> 8) & 0xFF,
                    COLOR_DEFAULT_RGB & 0xFF);
    bg_pixel = pack(fb, 0x10, 0x12, 0x18);

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
    draw_cell(cur_col, cur_row, true);
}

void console_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!ready) return;
    fg_pixel = pack(&fbinfo, r, g, b);
    redraw_all();
    draw_cell(cur_col, cur_row, true);
}

void console_set_bold(bool bold) {
    if (!ready || bold_enabled == bold) return;
    bold_enabled = bold;
    redraw_all();
    draw_cell(cur_col, cur_row, true);
}

void console_set_font(int font_id) {
    if (!ready || current_font == font_id) return;
    current_font = font_id;
    redraw_all();
    draw_cell(cur_col, cur_row, true);
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

    draw_cell(cur_col, cur_row, true);
}

void console_puts(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}