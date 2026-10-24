/* kernel/boot_info.h */
#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>

#define PROMPTOS_FRAMEBUFFER_RGB 0

struct promptos_framebuffer {
    uint8_t *address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

struct boot_info {
    struct promptos_framebuffer *framebuffer;
    char (*getc_fn)(void);
};

#endif