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
static struct promptos_framebuffer fbinfo;
static bool ready;
static bool bold_enabled = false;
static bool cursor_visible = true;

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
    fill_screen(bg_pixel);
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
    cursor_visible = true;
    draw_cell(cur_col, cur_row, true);
}

void console_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!ready) return;
    fg_pixel = pack(&fbinfo, r, g, b);
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_visible);
}

void console_set_bold(bool bold) {
    if (!ready || bold_enabled == bold) return;
    bold_enabled = bold;
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_visible);
}

bool console_get_bold(void) {
    return bold_enabled;
}

void console_set_scale(uint32_t new_scale) {
    if (!ready || new_scale < 1 || new_scale > 4 || scale == new_scale) return;
    scale = new_scale;
    update_metrics();
    redraw_all();
    draw_cell(cur_col, cur_row, cursor_visible);
}

uint32_t console_get_scale(void) {
    return scale;
}

void console_show_cursor(bool show) {
    if (!ready) return;
    cursor_visible = show;
    draw_cell(cur_col, cur_row, cursor_visible);
}

void console_toggle_cursor(void) {
    if (!ready) return;
    cursor_visible = !cursor_visible;
    draw_cell(cur_col, cur_row, cursor_visible);
}

void console_putc(char c) {
    if (!ready) return;

    // Remove inverted cursor block from old position
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
        cells[cur_row][cur_col] = 0;
        draw_cell(cur_col, cur_row, false);
    } else if (c >= 32 && c <= 126) {
        cells[cur_row][cur_col] = c;
        draw_cell(cur_col, cur_row, false);
        if (++cur_col >= cols) {
            newline();
        }
    }

    // Always restore solid inverted cursor at the new position
    cursor_visible = true;
    draw_cell(cur_col, cur_row, true);
}

void console_puts(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}