#pragma once

#include <stdint.h>
#include <stddef.h>

void debug_init(void);

void debug_putchar(char c);
void debug_write(const char *tag, const char *subsystem, const char *fmt, ...);
void debug_write_raw(const char *buf, size_t len);
void debug_rawnl(const char *buf);
bool debug_received(void);
char debug_read(void);
