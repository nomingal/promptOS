/* kernel/main.c */
#include <stdint.h>
#include <stdbool.h>
#include "boot_info.h"
#include "console.h"
#include "keyboard.h"
#include "power.h"
#include "shell.h"

/* Dedicated 64KB kernel stack aligned to 16 bytes */
uint8_t kernel_stack[65536] __attribute__((aligned(16), used));

void kernel_main(struct boot_info *info) {
    if (info && info->framebuffer) {
        console_init(info->framebuffer);
    }

    kbd_init_with_info(info);
    power_init_with_info(info);

    console_puts("promptOS booted!\n");
    console_puts("type 'help' for commands\n\n");

    shell_run();

    while (1) {
        __asm__ volatile("hlt");
    }
}

/* Naked Assembly entry point called by efi_main */
__attribute__((naked)) void _start(struct boot_info *info __attribute__((unused))) {
    __asm__ volatile(
        "lea kernel_stack + 65536(%%rip), %%rsp\n\t"
        "and $-16, %%rsp\n\t"
        "call kernel_main\n\t"
        "1:\n\t"
        "hlt\n\t"
        "jmp 1b\n\t"
        :
        :
        : "memory"
    );
}