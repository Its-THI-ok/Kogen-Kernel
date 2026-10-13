#include "debug.h"

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include <attributes.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value)
{
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t value;
	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

cold void debug_init(void)
{
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x80);
	outb(COM1 + 0, 0x03);
	outb(COM1 + 1, 0x00);
	outb(COM1 + 3, 0x03);
	outb(COM1 + 2, 0xC7);
	outb(COM1 + 4, 0x0B);
}

hot void debug_putchar(char c)
{
	if (c == '\n')
		debug_putchar('\r');

	while ((inb(COM1 + 5) & 0x20) == 0)
		;

	outb(COM1, c);
}

hot static void kuitoa(unsigned long long value, char *buf, int base, int upper)
{
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp[32];
	int i = 0;

	if (value == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}

	while (value) {
		tmp[i++] = digits[value % base];
		value /= base;
	}

	int j = 0;
	while (i > 0)
		buf[j++] = tmp[--i];
	buf[j] = '\0';
}

hot static int kvsnprintf(char *out, size_t size, const char *fmt, va_list args)
{
	if (!fmt)
		fmt = "(null fmt)";

	size_t pos = 0;
	char numbuf[32];

#define PUT(c)                                                                                     \
	do {                                                                                       \
		if (pos + 1 < size)                                                                \
			out[pos] = (c);                                                            \
		pos++;                                                                             \
	} while (0)

	while (*fmt) {
		if (*fmt != '%') {
			PUT(*fmt++);
			continue;
		}

		fmt++; /* skip '%' */

		/* --- flags --- */
		int zero_pad = 0;
		if (*fmt == '0') {
			zero_pad = 1;
			fmt++;
		}

		/* --- width --- */
		int width = 0;
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}

		/* --- length modifier --- */
		enum {
			LEN_NONE,
			LEN_HH,
			LEN_H,
			LEN_L,
			LEN_LL,
			LEN_Z
		} len = LEN_NONE;
		if (*fmt == 'h') {
			fmt++;
			if (*fmt == 'h') {
				len = LEN_HH;
				fmt++;
			} else
				len = LEN_H;
		} else if (*fmt == 'l') {
			fmt++;
			if (*fmt == 'l') {
				len = LEN_LL;
				fmt++;
			} else
				len = LEN_L;
		} else if (*fmt == 'z') {
			len = LEN_Z;
			fmt++;
		}

		switch (*fmt) {
		case 's': {
			const char *s = va_arg(args, const char *);
			if (!s)
				s = "(null)";
			size_t slen = 0;
			for (const char *p = s; *p; p++)
				slen++;
			for (size_t i = 0; i < (size_t)width && slen < (size_t)width; i++)
				PUT(' ');
			while (*s)
				PUT(*s++);
			break;
		}

		case 'd':
		case 'i': {
			long long val;
			switch (len) {
			case LEN_HH:
				val = (signed char)va_arg(args, int);
				break;
			case LEN_H:
				val = (short)va_arg(args, int);
				break;
			case LEN_L:
				val = va_arg(args, long);
				break;
			case LEN_LL:
				val = va_arg(args, long long);
				break;
			case LEN_Z:
				val = (long long)va_arg(args, size_t);
				break;
			default:
				val = va_arg(args, int);
				break;
			}

			int neg = 0;
			unsigned long long uval;
			if (val < 0) {
				neg = 1;
				uval = (unsigned long long)(-(val + 1)) +
					1ULL; /* avoid INT_MIN overflow */
			} else {
				uval = (unsigned long long)val;
			}

			kuitoa(uval, numbuf, 10, 0);
			size_t numlen = 0;
			for (char *p = numbuf; *p; p++)
				numlen++;

			size_t total = numlen + (neg ? 1 : 0);
			for (size_t i = total; i < (size_t)width; i++)
				PUT(zero_pad ? '0' : ' ');

			if (neg)
				PUT('-');
			for (char *p = numbuf; *p; p++)
				PUT(*p);
			break;
		}

		case 'u':
		case 'x':
		case 'X':
		case 'o': {
			unsigned long long val;
			switch (len) {
			case LEN_HH:
				val = (unsigned char)va_arg(args, unsigned int);
				break;
			case LEN_H:
				val = (unsigned short)va_arg(args, unsigned int);
				break;
			case LEN_L:
				val = va_arg(args, unsigned long);
				break;
			case LEN_LL:
				val = va_arg(args, unsigned long long);
				break;
			case LEN_Z:
				val = (unsigned long long)va_arg(args, size_t);
				break;
			default:
				val = va_arg(args, unsigned int);
				break;
			}

			int base = (*fmt == 'o') ? 8 : (*fmt == 'u') ? 10 : 16;
			int upper = (*fmt == 'X');

			kuitoa(val, numbuf, base, upper);
			size_t numlen = 0;
			for (char *p = numbuf; *p; p++)
				numlen++;

			for (size_t i = numlen; i < (size_t)width; i++)
				PUT(zero_pad ? '0' : ' ');

			for (char *p = numbuf; *p; p++)
				PUT(*p);
			break;
		}

		case 'p': {
			void *val = va_arg(args, void *);
			PUT('0');
			PUT('x');
			kuitoa((uintptr_t)val, numbuf, 16, 0);
			size_t numlen = 0;
			for (char *p = numbuf; *p; p++)
				numlen++;
			for (size_t i = numlen; i < 16; i++)
				PUT('0');
			for (char *p = numbuf; *p; p++)
				PUT(*p);
			break;
		}

		case 'c': {
			char c = (char)va_arg(args, int);
			PUT(c);
			break;
		}

		case '%':
			PUT('%');
			break;

		default:
			PUT('%');
			PUT(*fmt);
			break;
		}
		fmt++;
	}

	if (size > 0)
		out[pos < size ? pos : size - 1] = '\0';

#undef PUT

	return (int)pos;
}

hot void debug_write(const char *tag, const char *subsystem, const char *fmt, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, fmt);
	kvsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (tag != NULL) {
		debug_putchar('<');
		while (*tag)
			debug_putchar(*tag++);
		debug_putchar('>');
		debug_putchar(' ');
	}

	if (subsystem != NULL) {
		debug_putchar('[');
		while (*subsystem)
			debug_putchar(*subsystem++);
		debug_putchar(']');
		debug_putchar(' ');
	}

	const char *string = buffer;

	while (*string)
		debug_putchar(*string++);

	debug_putchar('\n');
}

hot void debug_write_raw(const char *buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
		debug_putchar(buf[i]);
}

hot void debug_rawnl(const char *buf)
{
	while (*buf) {
		if (*buf == '\n') {
			debug_putchar('\r');
			debug_putchar('\n');
		} else if (*buf == '\b') {
			debug_putchar('\b');
			debug_putchar(' ');
			debug_putchar('\b');
		} else {
			debug_putchar(*buf);
		}

		buf++;
	}
}

bool debug_received(void)
{
	return inb(COM1 + 5) & 0x01;
}

char debug_read(void)
{
	return inb(COM1);
}
