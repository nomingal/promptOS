#ifndef PROMPTOS_BOOT_H
#define PROMPTOS_BOOT_H

#include "limine_proto.h"

/* First framebuffer handed over by the bootloader, or NULL if none. */
const struct limine_framebuffer *boot_framebuffer(void);

#endif
