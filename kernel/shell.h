#ifndef PROMPTOS_SHELL_H
#define PROMPTOS_SHELL_H

/* Run one command line (the buffer is edited in place). */
void shell_exec(char *line);

/* Prompt loop: read a line, run it, repeat. Never returns. */
void shell_run(void) __attribute__((noreturn));

#endif
