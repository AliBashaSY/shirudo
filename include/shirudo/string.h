/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Bounded number parsing, string copies and formatting, DNS names, from
 * util/string.c.
 */

#ifndef SHIRUDO_STRING_H
#define SHIRUDO_STRING_H

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <shirudo/misc.h>

/* How many of the len bytes at s, from the first, are decimal digits. */
size_t digits(const char *s, size_t len);

/*
 * The number the len bytes at s spell, decimal for u64 and s64 and
 * hexadecimal for x64, within the bounds given: digits only, s64 with an
 * optional leading '-', and no blank, '+' or base prefix. Nothing past
 * len is read, so s need not be terminated. -EINVAL when the bytes are
 * not such a number, -ERANGE when it is out of bounds; *out is only
 * written on success.
 */
int parse_u64(const char *s, size_t len, __u64 max, __u64 *out);
int parse_x64(const char *s, size_t len, __u64 max, __u64 *out);
int parse_s64(const char *s, size_t len, __s64 min, __s64 max, __s64 *out);

/*
 * Copy the string src into the size bytes at dst, terminated whenever
 * size is not 0: its length, or -E2BIG when it did not fit and dst holds
 * as much of it as did.
 */
ssize_t strscpy(char *dst, const char *src, size_t size);

/*
 * snprintf() into the size bytes at buf: the length written, or -E2BIG,
 * with buf left empty, when the output did not fit. vstrsprintf() takes
 * the arguments as a va_list.
 */
int strsprintf(char *buf, size_t size, const char *fmt, ...)
	__printf(3, 4) __must_check;
int vstrsprintf(char *buf, size_t size, const char *fmt, va_list ap)
	__printf(3, 0) __must_check;

/*
 * A DNS host name: labels of letters, digits and inner hyphens, 1 to 63
 * bytes each and 253 in all. A name whose labels are all decimal or 0x
 * numbers is refused, as an IPv4 parser would read it as an address.
 */
bool dns_name_valid(const char *name);

/*
 * What the functions above replace, refused once the headers included
 * above have declared it, so that a later include of those still works;
 * util/string.c opts out with STRING_INTERNAL.
 */
#ifndef STRING_INTERNAL
# pragma GCC poison strtol strtoll strtoul strtoull strtoimax strtoumax
# pragma GCC poison strtod strtof strtold atoi atol atoll atof
# pragma GCC poison strcpy strncpy strcat strncat strlcpy
/* Fortified glibc makes these macros for clang; poison the names alone. */
# undef sprintf
# undef snprintf
# pragma GCC poison sprintf vsprintf snprintf vsnprintf
#endif

#endif /* SHIRUDO_STRING_H */
