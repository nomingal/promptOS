#include "keyboard.h"
#include "io.h"

#define KBD_DATA   0x60
#define KBD_STATUS 0x64   /* read: status, write: command */

#define STATUS_OUT_FULL 0x01
#define STATUS_IN_FULL  0x02
#define STATUS_AUX_DATA 0x20   /* byte came from the mouse port */

#define SPIN_LIMIT 100000

static const char map_plain[0x3A] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
    'b','n','m',',','.','/', 0,'*', 0,' '
};

static const char map_shift[0x3A] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X','C','V',
    'B','N','M','<','>','?', 0,'*', 0,' '
};

static int shift_down;
static int caps_lock;
static int skip_bytes;   /* bytes of an extended (0xE0 / 0xE1) sequence to ignore */

static int wait_input_empty(void) {
    for (int i = 0; i < SPIN_LIMIT; i++) {
        if (!(inb(KBD_STATUS) & STATUS_IN_FULL)) return 1;
    }
    return 0;
}

static int wait_output_full(void) {
    for (int i = 0; i < SPIN_LIMIT; i++) {
        if (inb(KBD_STATUS) & STATUS_OUT_FULL) return 1;
    }
    return 0;
}

void kbd_init(void) {
    /* Drop anything the firmware left in the output buffer. */
    for (int i = 0; i < 64 && (inb(KBD_STATUS) & STATUS_OUT_FULL); i++) {
        inb(KBD_DATA);
    }

    /* Make sure the controller translates scancodes to set 1 and has IRQs
       off (we poll). If the controller does not answer, carry on anyway. */
    if (!wait_input_empty()) return;
    outb(KBD_STATUS, 0x20);                  /* read config byte */
    if (!wait_output_full()) return;
    uint8_t config = inb(KBD_DATA);

    config |= (1 << 6);                      /* translation to set 1 */
    config &= (uint8_t)~((1 << 0) | (1 << 1) | (1 << 4));  /* no IRQs, port 1 clock on */

    if (!wait_input_empty()) return;
    outb(KBD_STATUS, 0x60);                  /* write config byte */
    if (!wait_input_empty()) return;
    outb(KBD_DATA, config);

    if (!wait_input_empty()) return;
    outb(KBD_STATUS, 0xAE);                  /* enable keyboard port */
}

char kbd_feed(uint8_t sc) {
    if (skip_bytes > 0) {
        skip_bytes--;
        return 0;
    }
    if (sc == 0xE0) { skip_bytes = 1; return 0; }   /* arrows, right ctrl, ... */
    if (sc == 0xE1) { skip_bytes = 5; return 0; }   /* pause/break */

    int released = sc & 0x80;
    sc &= 0x7F;

    if (sc == 0x2A || sc == 0x36) {          /* left / right shift */
        shift_down = !released;
        return 0;
    }
    if (released) return 0;
    if (sc == 0x3A) {                        /* caps lock toggles on press */
        caps_lock = !caps_lock;
        return 0;
    }
    if (sc >= sizeof map_plain) return 0;

    char c = shift_down ? map_shift[sc] : map_plain[sc];
    if (caps_lock) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z' && shift_down) c = (char)(c - 'A' + 'a');
    }
    return c;
}

char kbd_poll(void) {
    uint8_t status = inb(KBD_STATUS);
    if (!(status & STATUS_OUT_FULL)) return 0;
    uint8_t byte = inb(KBD_DATA);
    if (status & STATUS_AUX_DATA) return 0;  /* ignore mouse bytes */
    return kbd_feed(byte);
}

char kbd_getc(void) {
    for (;;) {
        char c = kbd_poll();
        if (c) return c;
        __asm__ volatile ("pause");
    }
}
