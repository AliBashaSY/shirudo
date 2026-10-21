/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Bounded number parsing, from util/string.c.
 */

#ifndef SHIRUDO_STRING_H
#define SHIRUDO_STRING_H

#include <stddef.h>

#include <shirudo/misc.h>

/* How many of the len bytes at s, from the first, are decimal digits. */
size_t shirudo_digits(const char *s, size_t len);

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

#endif /* SHIRUDO_STRING_H */
