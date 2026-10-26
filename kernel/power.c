/* kernel/power.c */
#include <stddef.h>
#include <stdint.h>
#include "boot_info.h"
#include "power.h"

static void (*uefi_shutdown_ptr)(void) = NULL;
static void (*uefi_reboot_ptr)(void) = NULL;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void io_wait(void) {
    outb(0x80, 0);
}

void power_init_with_info(struct boot_info *info) {
    if (info) {
        uefi_shutdown_ptr = info->shutdown_fn;
        uefi_reboot_ptr = info->reboot_fn;
    }
}

void power_reboot(void) {
    /* 1. Primary: Native UEFI ResetSystem (EfiResetCold) */
    if (uefi_reboot_ptr) {
        uefi_reboot_ptr();
    }

    /* 2. Fallback: Intel 12th Gen / Alder Lake PCI Reset Port 0xCF9 */
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    io_wait();

    /* 3. Fallback: PS/2 8042 Keyboard Controller Pulse Reset */
    outb(0x64, 0xFE);
    io_wait();

    /* CPU Halt loop if hardware takes time to power down */
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

void power_shutdown(void) {
    /* 1. Primary: Native UEFI ResetSystem (EfiResetShutdown) */
    if (uefi_shutdown_ptr) {
        uefi_shutdown_ptr();
    }

    /* 2. Fallback: QEMU / Bochs ACPI Power Off Ports */
    outw(0x604, 0x2000);  /* QEMU PM1a_CNT */
    outw(0xB004, 0x2000); /* Bochs / QEMU */

    /* CPU Halt fallback */
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}