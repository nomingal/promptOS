#ifndef PROMPTOS_LIMINE_PROTO_H
#define PROMPTOS_LIMINE_PROTO_H

/* The small part of the Limine boot protocol that promptOS uses.
   Values are from the official PROTOCOL.md. When the kernel needs more
   features (memory map, HHDM, ...), switch to the official limine.h. */

#include <stdint.h>

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL

/* Base revision tag: 2 magic numbers + requested revision. The bootloader
   sets the 3rd value to 0 when it supports the requested revision. */
#define LIMINE_BASE_REVISION_INIT(N) \
    { 0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, (N) }

#define LIMINE_FRAMEBUFFER_RGB 1

struct limine_framebuffer {
    void *address;            /* virtual address (higher-half direct map) */
    uint64_t width;
    uint64_t height;
    uint64_t pitch;           /* bytes per row */
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

#define LIMINE_FRAMEBUFFER_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL }

#endif
