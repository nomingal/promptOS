#include "boot.h"
#include "console.h"
#include "keyboard.h"
#include "shell.h"

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

/* Kernel entry point. Limine jumps here in 64-bit long mode with a stack. */
void _start(void) {
    if (!console_init(boot_framebuffer())) {
        halt_forever();          /* no usable framebuffer: nothing we can show */
    }
    kbd_init();

    console_puts("promptOS booted!\n");
    console_puts("type 'help' for commands\n\n");

    shell_run();
}
