// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Bounded number parsing, string copies and formatting.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <shirudo/string.h>

size_t digits(const char *s, size_t len)
{
	size_t n = 0;

	while (n < len && s[n] >= '0' && s[n] <= '9')
		n++;
	return n;
}

static int digit_value(char c, __u32 base)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (base == 16 && c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (base == 16 && c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int parse_base(const char *s, size_t len, __u32 base, __u64 max,
		      __u64 *out)
{
	__u64 v = 0;
	size_t i;

	if (!len)
		return -EINVAL;
	for (i = 0; i < len; i++) {
		int d = digit_value(s[i], base);

		if (d < 0)
			return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		__u64 d = (__u64)digit_value(s[i], base);

		if (d > max || v > (max - d) / base)
			return -ERANGE;
		v = v * base + d;
	}
	*out = v;
	return 0;
}

int parse_u64(const char *s, size_t len, __u64 max, __u64 *out)
{
	return parse_base(s, len, 10, max, out);
}

int parse_x64(const char *s, size_t len, __u64 max, __u64 *out)
{
	return parse_base(s, len, 16, max, out);
}

int parse_s64(const char *s, size_t len, __s64 min, __s64 max, __s64 *out)
{
	bool neg = len && s[0] == '-';
	__u64 v;
	__s64 r;
	int rc;

	rc = parse_base(s + neg, len - neg, 10,
			neg ? (__u64)INT64_MAX + 1 : (__u64)INT64_MAX, &v);
	if (rc)
		return rc;
	if (!neg)
		r = (__s64)v;
	else if (v == (__u64)INT64_MAX + 1)
		r = INT64_MIN;
	else
		r = -(__s64)v;
	if (r < min || r > max)
		return -ERANGE;
	*out = r;
	return 0;
}

ssize_t strscpy(char *dst, const char *src, size_t size)
{
	size_t len;

	if (!size)
		return -E2BIG;
	len = strnlen(src, size);
	if (len == size) {
		memcpy(dst, src, size - 1);
		dst[size - 1] = '\0';
		return -E2BIG;
	}
	memcpy(dst, src, len + 1);
	return (ssize_t)len;
}

int vstrsprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	int n = vsnprintf(buf, size, fmt, ap);

	if (n >= 0 && (size_t)n < size)
		return n;
	if (size)
		buf[0] = '\0';
	return -E2BIG;
}

int strsprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vstrsprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}
