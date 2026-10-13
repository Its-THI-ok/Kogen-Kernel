#pragma once

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * memory functions
 * ============================================================ */

static inline void *memset(void *dst, int value, size_t len)
{
	uint8_t *d = (uint8_t *)dst;
	uint8_t v = (uint8_t)value;

	for (size_t i = 0; i < len; i++)
		d[i] = v;

	return dst;
}

static inline void *memzero(void *dst, size_t len)
{
	return memset(dst, 0, len);
}

static inline void *memcpy(void *dst, const void *src, size_t len)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	for (size_t i = 0; i < len; i++)
		d[i] = s[i];

	return dst;
}

static inline void *memmove(void *dst, const void *src, size_t len)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	if (d == s || len == 0)
		return dst;

	if (d < s) {
		for (size_t i = 0; i < len; i++)
			d[i] = s[i];
	} else {
		for (size_t i = len; i > 0; i--)
			d[i - 1] = s[i - 1];
	}

	return dst;
}

static inline int memcmp(const void *a, const void *b, size_t len)
{
	const uint8_t *pa = (const uint8_t *)a;
	const uint8_t *pb = (const uint8_t *)b;

	for (size_t i = 0; i < len; i++) {
		if (pa[i] != pb[i])
			return (int)pa[i] - (int)pb[i];
	}

	return 0;
}

static inline void *memchr(const void *buf, int value, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	uint8_t v = (uint8_t)value;

	for (size_t i = 0; i < len; i++) {
		if (p[i] == v)
			return (void *)(p + i);
	}

	return NULL;
}

static inline void *memrchr(const void *buf, int value, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	uint8_t v = (uint8_t)value;

	for (size_t i = len; i > 0; i--) {
		if (p[i - 1] == v)
			return (void *)(p + i - 1);
	}

	return NULL;
}

/* ============================================================
 * length / measurement
 * ============================================================ */

static inline size_t strlen(const char *s)
{
	size_t len = 0;

	while (s[len] != '\0')
		len++;

	return len;
}

static inline size_t strnlen(const char *s, size_t max)
{
	size_t len = 0;

	while (len < max && s[len] != '\0')
		len++;

	return len;
}

/* ============================================================
 * copy / concat
 * ============================================================ */

static inline char *strcpy(char *dst, const char *src)
{
	size_t i = 0;

	while (src[i] != '\0') {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';

	return dst;
}

static inline char *strncpy(char *dst, const char *src, size_t n)
{
	size_t i = 0;

	while (i < n && src[i] != '\0') {
		dst[i] = src[i];
		i++;
	}
	while (i < n) {
		dst[i] = '\0';
		i++;
	}

	return dst;
}

/* like strncpy, but always NUL-terminates within n bytes and
 * returns the length it tried to copy (safer to reason about
 * for callers who don't want strncpy's zero-pad footgun) */
static inline size_t strlcpy(char *dst, const char *src, size_t n)
{
	size_t srclen = strlen(src);

	if (n == 0)
		return srclen;

	size_t copy_len = (srclen < n - 1) ? srclen : n - 1;

	memcpy(dst, src, copy_len);
	dst[copy_len] = '\0';

	return srclen;
}

static inline char *strcat(char *dst, const char *src)
{
	size_t dlen = strlen(dst);
	strcpy(dst + dlen, src);
	return dst;
}

static inline char *strncat(char *dst, const char *src, size_t n)
{
	size_t dlen = strlen(dst);
	size_t i = 0;

	while (i < n && src[i] != '\0') {
		dst[dlen + i] = src[i];
		i++;
	}
	dst[dlen + i] = '\0';

	return dst;
}

/* ============================================================
 * comparison
 * ============================================================ */

static inline int strcmp(const char *a, const char *b)
{
	size_t i = 0;

	while (a[i] != '\0' && a[i] == b[i])
		i++;

	return (unsigned char)a[i] - (unsigned char)b[i];
}

static inline int strncmp(const char *a, const char *b, size_t n)
{
	size_t i = 0;

	while (i < n && a[i] != '\0' && a[i] == b[i])
		i++;

	if (i == n)
		return 0;

	return (unsigned char)a[i] - (unsigned char)b[i];
}

static inline bool str_eq(const char *a, const char *b)
{
	return strcmp(a, b) == 0;
}

/* ============================================================
 * search
 * ============================================================ */

static inline char *strchr(const char *s, int c)
{
	char ch = (char)c;

	while (*s != '\0') {
		if (*s == ch)
			return (char *)s;
		s++;
	}

	if (ch == '\0')
		return (char *)s;

	return NULL;
}

static inline char *strrchr(const char *s, int c)
{
	char ch = (char)c;
	const char *last = NULL;

	while (*s != '\0') {
		if (*s == ch)
			last = s;
		s++;
	}

	if (ch == '\0')
		return (char *)s;

	return (char *)last;
}

static inline char *strstr(const char *haystack, const char *needle)
{
	if (*needle == '\0')
		return (char *)haystack;

	size_t needle_len = strlen(needle);

	for (const char *h = haystack; *h != '\0'; h++) {
		size_t i = 0;
		while (i < needle_len && h[i] != '\0' && h[i] == needle[i])
			i++;
		if (i == needle_len)
			return (char *)h;
	}

	return NULL;
}

static inline size_t strspn(const char *s, const char *accept)
{
	size_t count = 0;

	while (s[count] != '\0' && strchr(accept, s[count]) != NULL)
		count++;

	return count;
}

static inline size_t strcspn(const char *s, const char *reject)
{
	size_t count = 0;

	while (s[count] != '\0' && strchr(reject, s[count]) == NULL)
		count++;

	return count;
}

static inline char *strpbrk(const char *s, const char *accept)
{
	while (*s != '\0') {
		if (strchr(accept, *s) != NULL)
			return (char *)s;
		s++;
	}

	return NULL;
}

/* ============================================================
 * tokenizing (stateful, single-threaded use only — no internal
 * lock, caller owns the string being tokenized)
 * ============================================================ */

static inline char *strtok_r(char *str, const char *delim, char **saveptr)
{
	char *s = str ? str : *saveptr;

	if (!s)
		return NULL;

	s += strspn(s, delim);

	if (*s == '\0') {
		*saveptr = NULL;
		return NULL;
	}

	char *token_start = s;
	s += strcspn(s, delim);

	if (*s != '\0') {
		*s = '\0';
		s++;
	}

	*saveptr = s;

	return token_start;
}

/* ============================================================
 * char classification (subset of ctype.h, freestanding-safe)
 * ============================================================ */

static inline int is_digit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int is_alpha(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int is_alnum(int c)
{
	return is_alpha(c) || is_digit(c);
}

static inline int is_space(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static inline int is_upper(int c)
{
	return c >= 'A' && c <= 'Z';
}

static inline int is_lower(int c)
{
	return c >= 'a' && c <= 'z';
}

static inline int to_upper(int c)
{
	return is_lower(c) ? c - ('a' - 'A') : c;
}

static inline int to_lower(int c)
{
	return is_upper(c) ? c + ('a' - 'A') : c;
}

/* ============================================================
 * number <-> string
 * ============================================================ */

static inline long strtol_simple(const char *s, char **endptr, int base)
{
	while (is_space(*s))
		s++;

	int neg = 0;
	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if (base == 0) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			base = 16;
			s += 2;
		} else if (s[0] == '0' && s[1] != '\0') {
			base = 8;
			s++;
		} else {
			base = 10;
		}
	} else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
	}

	long result = 0;

	while (*s != '\0') {
		int digit;

		if (is_digit(*s))
			digit = *s - '0';
		else if (is_alpha(*s))
			digit = to_lower(*s) - 'a' + 10;
		else
			break;

		if (digit >= base)
			break;

		result = result * base + digit;
		s++;
	}

	if (endptr)
		*endptr = (char *)s;

	return neg ? -result : result;
}

static inline int atoi(const char *s)
{
	return (int)strtol_simple(s, NULL, 10);
}

static inline long atol(const char *s)
{
	return strtol_simple(s, NULL, 10);
}

/* buf must be at least 21 bytes for the worst case (INT64_MIN + NUL) */
static inline char *itoa(long long value, char *buf, int base)
{
	static const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	char tmp[64];
	int i = 0;
	int neg = 0;

	unsigned long long uval;

	if (base == 10 && value < 0) {
		neg = 1;
		uval = (unsigned long long)(-(value + 1)) + 1ULL;
	} else {
		uval = (unsigned long long)value;
	}

	if (uval == 0) {
		tmp[i++] = '0';
	} else {
		while (uval) {
			tmp[i++] = digits[uval % (unsigned)base];
			uval /= (unsigned)base;
		}
	}

	if (neg)
		tmp[i++] = '-';

	int j = 0;
	while (i > 0)
		buf[j++] = tmp[--i];
	buf[j] = '\0';

	return buf;
}

/* ============================================================
 * misc
 * ============================================================ */

static inline void strrev(char *s)
{
	size_t len = strlen(s);
	for (size_t i = 0; i < len / 2; i++) {
		char tmp = s[i];
		s[i] = s[len - 1 - i];
		s[len - 1 - i] = tmp;
	}
}

static inline bool str_starts_with(const char *s, const char *prefix)
{
	while (*prefix != '\0') {
		if (*s != *prefix)
			return false;
		s++;
		prefix++;
	}
	return true;
}

static inline bool str_ends_with(const char *s, const char *suffix)
{
	size_t slen = strlen(s);
	size_t suflen = strlen(suffix);

	if (suflen > slen)
		return false;

	return memcmp(s + (slen - suflen), suffix, suflen) == 0;
}
