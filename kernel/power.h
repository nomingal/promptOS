/* kernel/power.h */
#ifndef PROMPTOS_POWER_H
#define PROMPTOS_POWER_H

struct boot_info;

void power_init_with_info(struct boot_info *info);
void power_reboot(void);
void power_shutdown(void);

#endif