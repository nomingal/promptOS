#include "console.h"
#include "io.h"
#include "power.h"

static void delay(void) {
    for (unsigned i = 0; i < 200000; i++) {
        io_wait();
    }
}

static void halt_forever(void) __attribute__((noreturn));
static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void power_reboot(void) {
    __asm__ volatile ("cli");

    /* 1. Pulse the CPU reset line through the keyboard controller. */
    for (unsigned i = 0; i < 100000 && (inb(0x64) & 0x02); i++) {
        /* wait for the controller's input buffer to empty */
    }
    outb(0x64, 0xFE);
    delay();

    /* 2. Chipset reset control register (most Intel chipsets, QEMU q35). */
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    delay();

    /* 3. Last resort: an empty IDT plus an exception ends in a triple
          fault, which resets the CPU. */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) empty_idt = {0, 0};
    __asm__ volatile ("lidt %0; int3" : : "m"(empty_idt));

    halt_forever();
}

void power_shutdown(void) {
    __asm__ volatile ("cli");

    /* Emulator power-off ports. QEMU and VirtualBox exit as soon as they
       see their value. Real PCs ignore these writes. */
    outw(0x604, 0x2000);     /* QEMU (q35, current i440fx) */
    delay();
    outw(0xB004, 0x2000);    /* Bochs and old QEMU */
    delay();
    outw(0x4004, 0x3400);    /* VirtualBox */
    delay();

    /* Real hardware needs ACPI for this, which promptOS doesn't have yet. */
    console_puts("power-off is not supported on this machine yet.\n");
    console_puts("the system is halted; it is safe to turn the power off.\n");
    halt_forever();
}
