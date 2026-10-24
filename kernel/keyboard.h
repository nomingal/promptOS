/* kernel/keyboard.h */
#ifndef PROMPTOS_KEYBOARD_H
#define PROMPTOS_KEYBOARD_H

struct boot_info;

void kbd_init(void);
void kbd_init_with_info(struct boot_info *info);
char kbd_getc(void);

#endif