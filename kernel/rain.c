/* kernel/rain.c */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "keyboard.h"
#include "rain.h"

#define MAX_RAIN_COLS 256

typedef struct {
    int y;          /* Row position of drop head */
    int len;        /* Length of drop trail */
    int speed;      /* Falling speed step threshold */
    int step_cnt;   /* Delay counter */
    bool active;    /* Whether drop is currently falling */
} drop_t;

static uint32_t seed = 0x9241A831;

static uint32_t rand_num(void) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

static char rand_char(void) {
    return (char)(33 + (rand_num() % 94));
}

static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t speed_to_interval(int level) {
    switch (level) {
        case 1: return 120000000ULL; /* Very Slow */
        case 2: return 85000000ULL;  /* Slow (Default) */
        case 3: return 55000000ULL;  /* Medium */
        case 4: return 35000000ULL;  /* Fast */
        case 5: return 20000000ULL;  /* Very Fast */
        default: return 85000000ULL;
    }
}

void rain_run(int initial_speed) {
    uint32_t cols = console_get_cols();
    uint32_t rows = console_get_rows();

    if (cols > MAX_RAIN_COLS) cols = MAX_RAIN_COLS;

    static drop_t drops[MAX_RAIN_COLS];

    for (uint32_t c = 0; c < cols; c++) {
        drops[c].y = -1;
        drops[c].len = 0;
        drops[c].speed = 1;
        drops[c].step_cnt = 0;
        drops[c].active = false;
    }

    if (initial_speed < 1 || initial_speed > 5) {
        initial_speed = 2; /* Default slow speed */
    }

    int current_speed_level = initial_speed;
    uint64_t tick_interval = speed_to_interval(current_speed_level);

    console_clear();

    /* Drain pending keyboard input */
    while (kbd_getc() != 0);

    uint64_t last_tick = read_tsc();

    for (;;) {
        char key = kbd_getc();
        if (key != 0) {
            if (key == 's' || key == 'S') {
                /* Cycle speed level: 1 -> 2 -> 3 -> 4 -> 5 -> 1 */
                current_speed_level = (current_speed_level % 5) + 1;
                tick_interval = speed_to_interval(current_speed_level);
            } else {
                /* Pressing any other key exits matrix rain */
                break;
            }
        }

        uint64_t now = read_tsc();
        if (now - last_tick < tick_interval) {
            for (volatile int d = 0; d < 1000; d++);
            continue;
        }
        last_tick = now;

        for (uint32_t c = 0; c < cols; c++) {
            if (!drops[c].active) {
                if ((rand_num() % 100) < 4) { /* Random spawn chance */
                    drops[c].active = true;
                    drops[c].y = 0;
                    drops[c].len = 8 + (rand_num() % 16);
                    drops[c].speed = 1 + (rand_num() % 2);
                    drops[c].step_cnt = 0;
                }
                continue;
            }

            drops[c].step_cnt++;
            if (drops[c].step_cnt < drops[c].speed) {
                continue;
            }
            drops[c].step_cnt = 0;

            int head = drops[c].y;

            /* Clear tail */
            int tail = head - drops[c].len;
            if (tail >= 0 && (uint32_t)tail < rows) {
                console_draw_char(c, (uint32_t)tail, ' ', 0, 0, 0);
            }

            /* Render green fading trail */
            for (int k = 1; k < drops[c].len; k++) {
                int body_y = head - k;
                if (body_y >= 0 && (uint32_t)body_y < rows) {
                    uint8_t g = (uint8_t)(255 - (k * 220 / drops[c].len));
                    if (g < 40) g = 40;

                    char ch = (rand_num() % 5 == 0) ? rand_char() : (char)('0' + (rand_num() % 10));
                    console_draw_char(c, (uint32_t)body_y, ch, 0, g, 20);
                }
            }

            /* Render bright white/lime drop head */
            if (head >= 0 && (uint32_t)head < rows) {
                console_draw_char(c, (uint32_t)head, rand_char(), 220, 255, 220);
            }

            drops[c].y++;

            if (drops[c].y - drops[c].len >= (int)rows) {
                drops[c].active = false;
            }
        }
    }

    /* Drain input and restore prompt console */
    while (kbd_getc() != 0);
    console_clear();
}