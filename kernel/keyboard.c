/* kernel/keyboard.c */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "boot_info.h"
#include "keyboard.h"

static char (*uefi_getc_ptr)(void) = NULL;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void kbd_init_with_info(struct boot_info *info) {
    if (info) {
        uefi_getc_ptr = info->getc_fn;
    }
}

void kbd_init(void) {
    /* Standard PS/2 fallback initialization */
}

char kbd_getc(void) {
    /* 1. Try UEFI input protocol first (native driver for Aorus 17 XE4 and modern laptops) */
    if (uefi_getc_ptr) {
        char c = uefi_getc_ptr();
        if (c != 0) {
            return c;
        }
    }

    /* 2. Legacy PS/2 hardware fallback */
    uint8_t status = inb(0x64);
    if (status == 0xFF || !(status & 1)) {
        return 0;
    }

    uint8_t scancode = inb(0x60);
    if (scancode & 0x80) {
        return 0;
    }

    static const char scancode_map[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
      '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
       '*',   0, ' '
    };

    if (scancode < 128) {
        return scancode_map[scancode];
    }

    return 0;
}