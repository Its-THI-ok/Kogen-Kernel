/*
 * strx.h — high-level string manipulation, built on top of the classic
 *          string.h primitives instead of duplicating them.
 *
 * String views (zero-copy slices), splitting, trimming, case conversion,
 * joining, replacing, padding, number <-> string conversion, a bounded
 * string builder, hashing, and basic UTF-8 helpers. No heap allocation
 * anywhere — every function that writes output takes a caller-supplied
 * buffer and capacity, and truncation never overflows: functions that
 * can't fit their result return SIZE_MAX instead.
 *
 * Naming convention:
 *   str_*   — operate on NUL-terminated `char *` / write into `char *`
 *   sv_*    — operate on str_view_t (non-owning {ptr, len} slice), read-only
 *   sb_*    — operate on str_builder_t (bounded, mutable string builder)
 *   utf8_*  — UTF-8 aware helpers
 *   is_.. / to_.. — character classification
 */

#ifndef _STRX_H
#define _STRX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/* ========================================================================
 * Core types
 * ======================================================================== */

/* Non-owning slice of a byte buffer. Does NOT need to be NUL-terminated. */
typedef struct {
    const char *ptr;
    size_t      len;
} str_view_t;

typedef str_view_t sv_t; /* short alias, used throughout */

/* Bounded, mutable string builder over a caller-owned buffer. Never
 * allocates; `cap` is the hard ceiling. `len` never exceeds `cap - 1` so
 * there's always room for a NUL terminator on demand via sb_cstr(). */
typedef struct {
    char   *buf;
    size_t  len;
    size_t  cap;
} str_builder_t;

typedef str_builder_t sb_t;

/* ========================================================================
 * Character classification (ctype.h replacement, ASCII-only by design)
 * ======================================================================== */
static inline bool is_punct(int c) {
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') ||
           (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}
static inline bool is_hex_digit(int c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static inline bool is_print(int c)     { return c >= 0x20 && c < 0x7F; }
static inline bool is_ctrl(int c)      { return (c >= 0 && c < 0x20) || c == 0x7F; }

/* value of a hex digit char, or -1 if not a hex digit */
static inline int hex_digit_value(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ========================================================================
 * Case-insensitive compare and safe copy/append (not in classic string.h)
 * ======================================================================== */

static inline int str_icmp(const char *a, const char *b) {
    while (*a && to_lower((unsigned char)*a) == to_lower((unsigned char)*b)) { a++; b++; }
    return to_lower((unsigned char)*a) - to_lower((unsigned char)*b);
}

/* strlcpy-style safe copy: always NUL-terminates if cap > 0, never
 * overflows, returns strlen(src) (so callers can detect truncation by
 * comparing the return value against cap). */
static inline size_t str_copy(char *dst, size_t cap, const char *src) {
    size_t n = strlen(src);
    if (cap > 0) {
        size_t to_copy = (n < cap - 1) ? n : cap - 1;
        memcpy(dst, src, to_copy);
        dst[to_copy] = '\0';
    }
    return n;
}

/* strlcat-style safe append: returns the total length it *would* be
 * (strlen(dst_initial) + strlen(src)), always NUL-terminates. */
static inline size_t str_cat(char *dst, size_t cap, const char *src) {
    size_t dlen = strlen(dst);
    size_t slen = strlen(src);
    if (dlen >= cap) return dlen + slen; /* dst already not terminated within cap */
    size_t room = cap - dlen - 1;
    size_t to_copy = (slen < room) ? slen : room;
    memcpy(dst + dlen, src, to_copy);
    dst[dlen + to_copy] = '\0';
    return dlen + slen;
}

/* ========================================================================
 * str_view_t construction & basic predicates
 * ======================================================================== */

static inline sv_t sv_from_parts(const char *ptr, size_t len) {
    sv_t s; s.ptr = ptr; s.len = len; return s;
}

static inline sv_t sv_from_cstr(const char *s) {
    return sv_from_parts(s, strlen(s));
}

static inline bool sv_is_empty(sv_t s) { return s.len == 0; }

static inline bool sv_is_blank(sv_t s) {
    for (size_t i = 0; i < s.len; i++) if (!is_space((unsigned char)s.ptr[i])) return false;
    return true;
}

static inline bool sv_equal(sv_t a, sv_t b) {
    if (a.len != b.len) return false;
    return a.len == 0 || memcmp(a.ptr, b.ptr, a.len) == 0;
}

static inline bool sv_equal_cstr(sv_t a, const char *b) {
    return sv_equal(a, sv_from_cstr(b));
}

static inline bool sv_equal_ignore_case(sv_t a, sv_t b) {
    if (a.len != b.len) return false;
    for (size_t i = 0; i < a.len; i++)
        if (to_lower((unsigned char)a.ptr[i]) != to_lower((unsigned char)b.ptr[i])) return false;
    return true;
}

static inline bool sv_starts_with(sv_t s, sv_t prefix) {
    if (prefix.len > s.len) return false;
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

static inline bool sv_starts_with_cstr(sv_t s, const char *prefix) {
    return sv_starts_with(s, sv_from_cstr(prefix));
}

static inline bool sv_ends_with(sv_t s, sv_t suffix) {
    if (suffix.len > s.len) return false;
    return memcmp(s.ptr + (s.len - suffix.len), suffix.ptr, suffix.len) == 0;
}

static inline bool sv_ends_with_cstr(sv_t s, const char *suffix) {
    return sv_ends_with(s, sv_from_cstr(suffix));
}

/* ========================================================================
 * Searching
 * ======================================================================== */

/* returns byte index of first match, or -1 if not found */
static inline int64_t sv_find(sv_t hay, sv_t needle) {
    if (needle.len == 0) return 0;
    if (needle.len > hay.len) return -1;
    for (size_t i = 0; i + needle.len <= hay.len; i++)
        if (memcmp(hay.ptr + i, needle.ptr, needle.len) == 0) return (int64_t)i;
    return -1;
}

static inline int64_t sv_find_cstr(sv_t hay, const char *needle) {
    return sv_find(hay, sv_from_cstr(needle));
}

static inline int64_t sv_find_char(sv_t hay, char c) {
    for (size_t i = 0; i < hay.len; i++) if (hay.ptr[i] == c) return (int64_t)i;
    return -1;
}

static inline int64_t sv_rfind_char(sv_t hay, char c) {
    for (size_t i = hay.len; i > 0; i--) if (hay.ptr[i - 1] == c) return (int64_t)(i - 1);
    return -1;
}

static inline bool sv_contains(sv_t hay, sv_t needle) { return sv_find(hay, needle) >= 0; }
static inline bool sv_contains_char(sv_t hay, char c) { return sv_find_char(hay, c) >= 0; }

/* counts non-overlapping occurrences of `needle` in `hay` */
static inline size_t sv_count(sv_t hay, sv_t needle) {
    if (needle.len == 0) return 0;
    size_t count = 0;
    size_t i = 0;
    while (i + needle.len <= hay.len) {
        if (memcmp(hay.ptr + i, needle.ptr, needle.len) == 0) { count++; i += needle.len; }
        else i++;
    }
    return count;
}

/* ========================================================================
 * Slicing & trimming (all zero-copy — return views into the same buffer)
 * ======================================================================== */

static inline sv_t sv_substr(sv_t s, size_t start, size_t len) {
    if (start >= s.len) return sv_from_parts(s.ptr + s.len, 0);
    size_t avail = s.len - start;
    if (len > avail) len = avail;
    return sv_from_parts(s.ptr + start, len);
}

static inline sv_t sv_trim_left(sv_t s) {
    while (s.len > 0 && is_space((unsigned char)s.ptr[0])) { s.ptr++; s.len--; }
    return s;
}

static inline sv_t sv_trim_right(sv_t s) {
    while (s.len > 0 && is_space((unsigned char)s.ptr[s.len - 1])) s.len--;
    return s;
}

static inline sv_t sv_trim(sv_t s) { return sv_trim_right(sv_trim_left(s)); }

/* strips a specific char from both ends, e.g. sv_trim_char(s, '"') */
static inline sv_t sv_trim_char(sv_t s, char c) {
    while (s.len > 0 && s.ptr[0] == c) { s.ptr++; s.len--; }
    while (s.len > 0 && s.ptr[s.len - 1] == c) s.len--;
    return s;
}

/* mutating in-place trim for a NUL-terminated buffer: shifts content left
 * and re-terminates. Returns the new length. */
static inline size_t str_trim_inplace(char *s) {
    size_t len = strlen(s);
    sv_t trimmed = sv_trim(sv_from_parts(s, len));
    if (trimmed.ptr != s) memmove(s, trimmed.ptr, trimmed.len);
    s[trimmed.len] = '\0';
    return trimmed.len;
}

/* ========================================================================
 * Case conversion
 * ======================================================================== */

static inline void str_lower_inplace(char *s) { for (; *s; s++) *s = (char)to_lower((unsigned char)*s); }
static inline void str_upper_inplace(char *s) { for (; *s; s++) *s = (char)to_upper((unsigned char)*s); }

/* write lowercased/uppercased copy of `src` into dst/cap (safe, truncating).
 * returns strlen(src), same convention as str_copy. */
static inline size_t str_lower(char *dst, size_t cap, const char *src) {
    size_t n = strlen(src);
    size_t to_copy = (cap == 0) ? 0 : ((n < cap - 1) ? n : cap - 1);
    for (size_t i = 0; i < to_copy; i++) dst[i] = (char)to_lower((unsigned char)src[i]);
    if (cap > 0) dst[to_copy] = '\0';
    return n;
}

static inline size_t str_upper(char *dst, size_t cap, const char *src) {
    size_t n = strlen(src);
    size_t to_copy = (cap == 0) ? 0 : ((n < cap - 1) ? n : cap - 1);
    for (size_t i = 0; i < to_copy; i++) dst[i] = (char)to_upper((unsigned char)src[i]);
    if (cap > 0) dst[to_copy] = '\0';
    return n;
}

/* ========================================================================
 * Splitting / tokenizing
 * ======================================================================== */

/* Splits `s` on every occurrence of `delim`, filling `out` (capacity
 * `max_out`) with views into the original buffer. Returns the number of
 * fields written to `out`. If there are more fields than `max_out`, the
 * last written field absorbs the remainder of the string (like most
 * "limit" split implementations), so no data is silently dropped. */
static inline size_t sv_split(sv_t s, char delim, sv_t *out, size_t max_out) {
    if (max_out == 0) return 0;
    size_t count = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.len; i++) {
        if (s.ptr[i] == delim) {
            if (count + 1 >= max_out) break; /* reserve last slot for remainder */
            out[count++] = sv_from_parts(s.ptr + start, i - start);
            start = i + 1;
        }
    }
    out[count++] = sv_from_parts(s.ptr + start, s.len - start);
    return count;
}

/* like sv_split, but splits on ANY char present in `delims` */
static inline size_t sv_split_any(sv_t s, const char *delims, sv_t *out, size_t max_out) {
    if (max_out == 0) return 0;
    size_t count = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.len; i++) {
        if (strchr(delims, s.ptr[i])) {
            if (count + 1 >= max_out) break;
            out[count++] = sv_from_parts(s.ptr + start, i - start);
            start = i + 1;
        }
    }
    out[count++] = sv_from_parts(s.ptr + start, s.len - start);
    return count;
}

/* Stateful tokenizer: no array limit, walks one token at a time.
 * Usage:
 *   sv_t cursor = sv_from_cstr("a,b,,c");
 *   sv_t tok;
 *   while (sv_next_token(&cursor, ',', &tok)) { ... use tok ... }
 * Returns false once the cursor is exhausted (cursor.ptr == NULL sentinel). */
static inline bool sv_next_token(sv_t *cursor, char delim, sv_t *out) {
    if (cursor->ptr == NULL) return false;
    int64_t idx = sv_find_char(*cursor, delim);
    if (idx < 0) {
        *out = *cursor;
        cursor->ptr = NULL;
        cursor->len = 0;
        return true;
    }
    *out = sv_from_parts(cursor->ptr, (size_t)idx);
    cursor->ptr += (size_t)idx + 1;
    cursor->len -= (size_t)idx + 1;
    return true;
}

/* splits into lines, transparently handling both "\n" and "\r\n" */
static inline size_t sv_split_lines(sv_t s, sv_t *out, size_t max_out) {
    size_t n = sv_split(s, '\n', out, max_out);
    for (size_t i = 0; i < n; i++) {
        if (out[i].len > 0 && out[i].ptr[out[i].len - 1] == '\r') out[i].len--;
    }
    return n;
}

/* ========================================================================
 * Joining, replacing, repeating, padding, reversing
 * (all write into caller buffers; all return SIZE_MAX if the result
 *  wouldn't fit in `cap`, and never write a partial/corrupt result)
 * ======================================================================== */

static inline size_t sv_join(char *dst, size_t cap, const sv_t *parts, size_t count, sv_t sep) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += parts[i].len;
        if (i + 1 < count) total += sep.len;
    }
    if (total + 1 > cap) return SIZE_MAX;
    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        memcpy(dst + pos, parts[i].ptr, parts[i].len);
        pos += parts[i].len;
        if (i + 1 < count) { memcpy(dst + pos, sep.ptr, sep.len); pos += sep.len; }
    }
    dst[pos] = '\0';
    return pos;
}

static inline size_t str_replace_all(char *dst, size_t cap, sv_t src, sv_t from, sv_t to) {
    if (from.len == 0) return SIZE_MAX; /* replacing empty needle is ill-defined here */
    size_t pos = 0;
    size_t i = 0;
    while (i < src.len) {
        bool matched = (i + from.len <= src.len) && memcmp(src.ptr + i, from.ptr, from.len) == 0;
        if (matched) {
            if (pos + to.len + 1 > cap) return SIZE_MAX;
            memcpy(dst + pos, to.ptr, to.len);
            pos += to.len;
            i += from.len;
        } else {
            if (pos + 1 + 1 > cap) return SIZE_MAX;
            dst[pos++] = src.ptr[i++];
        }
    }
    dst[pos] = '\0';
    return pos;
}

static inline size_t str_repeat(char *dst, size_t cap, sv_t s, size_t times) {
    size_t total = s.len * times;
    if (total + 1 > cap) return SIZE_MAX;
    for (size_t i = 0; i < times; i++) memcpy(dst + i * s.len, s.ptr, s.len);
    dst[total] = '\0';
    return total;
}

static inline size_t str_pad_left(char *dst, size_t cap, sv_t src, size_t width, char pad) {
    if (src.len >= width) {
        if (src.len + 1 > cap) return SIZE_MAX;
        memcpy(dst, src.ptr, src.len);
        dst[src.len] = '\0';
        return src.len;
    }
    size_t pad_n = width - src.len;
    if (width + 1 > cap) return SIZE_MAX;
    memset(dst, pad, pad_n);
    memcpy(dst + pad_n, src.ptr, src.len);
    dst[width] = '\0';
    return width;
}

static inline size_t str_pad_right(char *dst, size_t cap, sv_t src, size_t width, char pad) {
    if (src.len >= width) {
        if (src.len + 1 > cap) return SIZE_MAX;
        memcpy(dst, src.ptr, src.len);
        dst[src.len] = '\0';
        return src.len;
    }
    size_t pad_n = width - src.len;
    if (width + 1 > cap) return SIZE_MAX;
    memcpy(dst, src.ptr, src.len);
    memset(dst + src.len, pad, pad_n);
    dst[width] = '\0';
    return width;
}

/* in-place reversal of a NUL-terminated buffer; returns its length */
static inline size_t str_reverse_inplace(char *s) {
    size_t n = strlen(s);
    for (size_t i = 0, j = n; i < j; i++, j--) {
        char t = s[i]; s[i] = s[j - 1]; s[j - 1] = t;
    }
    return n;
}

/* ========================================================================
 * Content checks
 * ======================================================================== */

static inline bool sv_is_numeric(sv_t s) {
    if (s.len == 0) return false;
    for (size_t i = 0; i < s.len; i++) if (!is_digit((unsigned char)s.ptr[i])) return false;
    return true;
}

static inline bool sv_is_alpha(sv_t s) {
    if (s.len == 0) return false;
    for (size_t i = 0; i < s.len; i++) if (!is_alpha((unsigned char)s.ptr[i])) return false;
    return true;
}

static inline bool sv_is_alnum(sv_t s) {
    if (s.len == 0) return false;
    for (size_t i = 0; i < s.len; i++) if (!is_alnum((unsigned char)s.ptr[i])) return false;
    return true;
}

/* valid C-style identifier: [A-Za-z_][A-Za-z0-9_]* */
static inline bool sv_is_identifier(sv_t s) {
    if (s.len == 0) return false;
    char c0 = s.ptr[0];
    if (!is_alpha((unsigned char)c0) && c0 != '_') return false;
    for (size_t i = 1; i < s.len; i++) {
        char c = s.ptr[i];
        if (!is_alnum((unsigned char)c) && c != '_') return false;
    }
    return true;
}

/* ========================================================================
 * Number <-> string conversion (no libc, no stdio)
 * ======================================================================== */

static const char STR_DIGITS_LOWER[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char STR_DIGITS_UPPER[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* writes unsigned `v` in the given `base` (2..36) into dst/cap.
 * returns the number of characters written (excl. NUL), or SIZE_MAX if it
 * doesn't fit. */
static inline size_t uint_to_str_base(char *dst, size_t cap, uint64_t v, int base, bool uppercase) {
    if (base < 2 || base > 36 || cap == 0) return SIZE_MAX;
    const char *digits = uppercase ? STR_DIGITS_UPPER : STR_DIGITS_LOWER;
    char tmp[64];
    size_t n = 0;
    if (v == 0) { tmp[n++] = '0'; }
    while (v > 0) {
        tmp[n++] = digits[v % (uint64_t)base];
        v /= (uint64_t)base;
    }
    if (n + 1 > cap) return SIZE_MAX;
    for (size_t i = 0; i < n; i++) dst[i] = tmp[n - 1 - i];
    dst[n] = '\0';
    return n;
}

static inline size_t uint_to_str(char *dst, size_t cap, uint64_t v) {
    return uint_to_str_base(dst, cap, v, 10, false);
}

static inline size_t int_to_str_base(char *dst, size_t cap, int64_t v, int base, bool uppercase) {
    if (cap == 0) return SIZE_MAX;
    if (v < 0) {
        if (cap < 2) return SIZE_MAX;
        dst[0] = '-';
        /* careful with INT64_MIN: negate via uint64 to avoid UB overflow */
        uint64_t mag = (uint64_t)(-(v + 1)) + 1;
        size_t n = uint_to_str_base(dst + 1, cap - 1, mag, base, uppercase);
        return (n == SIZE_MAX) ? SIZE_MAX : n + 1;
    }
    return uint_to_str_base(dst, cap, (uint64_t)v, base, uppercase);
}

static inline size_t int_to_str(char *dst, size_t cap, int64_t v) {
    return int_to_str_base(dst, cap, v, 10, false);
}

/* parses an unsigned integer from `s` in the given base. Returns false on
 * empty input or any invalid character (no partial-parse silently accepted). */
static inline bool sv_to_uint64_base(sv_t s, int base, uint64_t *out) {
    if (s.len == 0 || base < 2 || base > 36) return false;
    uint64_t result = 0;
    for (size_t i = 0; i < s.len; i++) {
        int d;
        char c = s.ptr[i];
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else return false;
        if (d >= base) return false;
        result = result * (uint64_t)base + (uint64_t)d;
    }
    *out = result;
    return true;
}

static inline bool sv_to_uint64(sv_t s, uint64_t *out) { return sv_to_uint64_base(s, 10, out); }

static inline bool sv_to_int64_base(sv_t s, int base, int64_t *out) {
    if (s.len == 0) return false;
    bool neg = false;
    size_t i = 0;
    if (s.ptr[0] == '+' || s.ptr[0] == '-') { neg = (s.ptr[0] == '-'); i = 1; }
    if (i >= s.len) return false;
    uint64_t mag;
    if (!sv_to_uint64_base(sv_substr(s, i, s.len - i), base, &mag)) return false;
    *out = neg ? -(int64_t)mag : (int64_t)mag;
    return true;
}

static inline bool sv_to_int64(sv_t s, int64_t *out) { return sv_to_int64_base(s, 10, out); }

/* Basic fixed-point double -> string (no exponent notation). Good enough
 * for logs/debug output; not a correctly-rounded dtoa. */
static inline size_t double_to_str(char *dst, size_t cap, double v, int precision) {
    if (cap == 0) return SIZE_MAX;
    if (precision < 0) precision = 6;
    size_t pos = 0;
    if (v < 0.0) { if (pos + 1 >= cap) return SIZE_MAX; dst[pos++] = '-'; v = -v; }

    /* integer part */
    uint64_t ip = (uint64_t)v;
    double frac = v - (double)ip;
    size_t n = uint_to_str_base(dst + pos, cap - pos, ip, 10, false);
    if (n == SIZE_MAX) return SIZE_MAX;
    pos += n;

    if (precision > 0) {
        if (pos + 1 >= cap) return SIZE_MAX;
        dst[pos++] = '.';
        for (int i = 0; i < precision; i++) {
            frac *= 10.0;
            int digit = (int)frac;
            if (digit > 9) digit = 9; /* guard against fp rounding edge cases */
            if (pos + 1 >= cap) return SIZE_MAX;
            dst[pos++] = (char)('0' + digit);
            frac -= (double)digit;
        }
    }
    dst[pos] = '\0';
    return pos;
}

/* Basic string -> double parser: [sign] digits [ '.' digits ]. No
 * exponent/scientific notation, no inf/nan literals. Returns false on
 * malformed input. */
static inline bool sv_to_double(sv_t s, double *out) {
    if (s.len == 0) return false;
    size_t i = 0;
    bool neg = false;
    if (s.ptr[0] == '+' || s.ptr[0] == '-') { neg = (s.ptr[0] == '-'); i = 1; }
    if (i >= s.len) return false;

    double result = 0.0;
    bool any_digits = false;
    while (i < s.len && is_digit((unsigned char)s.ptr[i])) {
        result = result * 10.0 + (double)(s.ptr[i] - '0');
        i++; any_digits = true;
    }
    if (i < s.len && s.ptr[i] == '.') {
        i++;
        double scale = 0.1;
        while (i < s.len && is_digit((unsigned char)s.ptr[i])) {
            result += (double)(s.ptr[i] - '0') * scale;
            scale *= 0.1;
            i++; any_digits = true;
        }
    }
    if (!any_digits || i != s.len) return false;
    *out = neg ? -result : result;
    return true;
}

/* ========================================================================
 * str_builder_t — bounded, allocation-free string builder
 * ======================================================================== */

static inline void sb_init(sb_t *sb, char *buf, size_t cap) {
    sb->buf = buf;
    sb->len = 0;
    sb->cap = cap;
    if (cap > 0) buf[0] = '\0';
}

static inline void sb_clear(sb_t *sb) {
    sb->len = 0;
    if (sb->cap > 0) sb->buf[0] = '\0';
}

static inline bool sb_is_full(const sb_t *sb) { return sb->len + 1 >= sb->cap; }

static inline bool sb_append_char(sb_t *sb, char c) {
    if (sb->len + 1 >= sb->cap) return false; /* no room incl. NUL */
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
    return true;
}

static inline bool sb_append_sv(sb_t *sb, sv_t s) {
    if (sb->len + s.len + 1 > sb->cap) return false;
    memcpy(sb->buf + sb->len, s.ptr, s.len);
    sb->len += s.len;
    sb->buf[sb->len] = '\0';
    return true;
}

static inline bool sb_append_cstr(sb_t *sb, const char *s) {
    return sb_append_sv(sb, sv_from_cstr(s));
}

static inline bool sb_append_repeat(sb_t *sb, char c, size_t n) {
    if (sb->len + n + 1 > sb->cap) return false;
    memset(sb->buf + sb->len, c, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
    return true;
}

static inline bool sb_append_int(sb_t *sb, int64_t v) {
    char tmp[32];
    size_t n = int_to_str(tmp, sizeof(tmp), v);
    if (n == SIZE_MAX) return false;
    return sb_append_sv(sb, sv_from_parts(tmp, n));
}

static inline bool sb_append_uint(sb_t *sb, uint64_t v) {
    char tmp[32];
    size_t n = uint_to_str(tmp, sizeof(tmp), v);
    if (n == SIZE_MAX) return false;
    return sb_append_sv(sb, sv_from_parts(tmp, n));
}

static inline bool sb_append_hex(sb_t *sb, uint64_t v, bool uppercase) {
    char tmp[20];
    size_t n = uint_to_str_base(tmp, sizeof(tmp), v, 16, uppercase);
    if (n == SIZE_MAX) return false;
    return sb_append_sv(sb, sv_from_parts(tmp, n));
}

static inline const char *sb_cstr(sb_t *sb) { return sb->buf; /* always NUL-terminated by construction */ }
static inline sv_t sb_view(const sb_t *sb) { return sv_from_parts(sb->buf, sb->len); }

/* ========================================================================
 * Hashing (useful as a key hash for any hash-table implementation)
 * ======================================================================== */

static inline uint32_t str_hash_fnv1a32(sv_t s) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < s.len; i++) {
        h ^= (unsigned char)s.ptr[i];
        h *= 0x01000193u;
    }
    return h;
}

static inline uint64_t str_hash_fnv1a64(sv_t s) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < s.len; i++) {
        h ^= (unsigned char)s.ptr[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

/* ========================================================================
 * UTF-8 helpers
 * (byte length via strlen/sv.len is NOT codepoint count — these are for
 *  when that distinction actually matters, e.g. terminal width, text UIs)
 * ======================================================================== */

/* decodes one UTF-8 codepoint starting at s[0]. Returns the number of
 * bytes consumed (1-4), or 0 if the byte sequence is invalid/truncated. */
static inline int utf8_decode(const char *s, size_t avail, uint32_t *cp_out) {
    if (avail == 0) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) { *cp_out = c0; return 1; }

    int len; uint32_t cp; unsigned char mask;
    if ((c0 & 0xE0) == 0xC0) { len = 2; cp = c0 & 0x1F; mask = 0xE0; }
    else if ((c0 & 0xF0) == 0xE0) { len = 3; cp = c0 & 0x0F; mask = 0xF0; }
    else if ((c0 & 0xF8) == 0xF0) { len = 4; cp = c0 & 0x07; mask = 0xF8; }
    else return 0; /* invalid leading byte */
    (void)mask;

    if (avail < (size_t)len) return 0;
    for (int i = 1; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0xC0) != 0x80) return 0; /* invalid continuation byte */
        cp = (cp << 6) | (c & 0x3F);
    }
    *cp_out = cp;
    return len;
}

/* counts codepoints (not bytes) in a UTF-8 buffer. Returns SIZE_MAX if the
 * buffer contains invalid UTF-8. */
static inline size_t utf8_strlen(sv_t s) {
    size_t count = 0;
    size_t i = 0;
    while (i < s.len) {
        uint32_t cp;
        int n = utf8_decode(s.ptr + i, s.len - i, &cp);
        if (n == 0) return SIZE_MAX;
        i += (size_t)n;
        count++;
    }
    return count;
}

static inline bool utf8_is_valid(sv_t s) { return utf8_strlen(s) != SIZE_MAX; }

#ifdef __cplusplus
}
#endif

#endif /* _STRX_H */
