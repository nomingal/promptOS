#ifndef PROMPTOS_KEYBOARD_H
#define PROMPTOS_KEYBOARD_H

#include <stdint.h>

void kbd_init(void);

/* Feed one raw scancode (set 1) through the decoder.
   Returns an ASCII character, or 0 if the byte did not produce one. */
char kbd_feed(uint8_t scancode);

/* Poll the PS/2 controller: returns a character or 0 if none is ready. */
char kbd_poll(void);

/* Block (busy-wait) until a character is typed. */
char kbd_getc(void);

#endif
