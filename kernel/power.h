#ifndef PROMPTOS_POWER_H
#define PROMPTOS_POWER_H

/* Restart the machine. Does not return. */
void power_reboot(void) __attribute__((noreturn));

/* Power the machine off. Does not return. On hardware where power-off is
   not supported yet, prints a message and halts instead. */
void power_shutdown(void) __attribute__((noreturn));

#endif
