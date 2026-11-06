// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * util/hex.c for every input of up to LEN_MAX bytes: an encoding is
 * lowercase hex that decodes back to its input, and a decode takes
 * exactly 2 * n hex digits of either case and yields their value.
 */

#include <errno.h>
#include <stdbool.h>

#include "proof.h"
#include "../../util/hex.c"

#define LEN_MAX	4

static int digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static void encoded(void)
{
	__u8 *in, *back;
	size_t n, i;
	char *out;

	__CPROVER_assume(n <= LEN_MAX);
	in = any_bytes(n);
	out = any_bytes(2 * n + 1);
	back = any_bytes(n);
	hex_encode(in, n, out);
	__CPROVER_assert(!out[2 * n], "terminated");
	for (i = 0; i < 2 * n; i++)
		__CPROVER_assert((out[i] >= '0' && out[i] <= '9') ||
				 (out[i] >= 'a' && out[i] <= 'f'),
				 "lowercase hex digits");
	__CPROVER_assert(!hex_decode(out, back, n) && !memcmp(back, in, n),
			 "decodes back to its input");
}

static void decoded(void)
{
	size_t len, n, i;
	bool hex = true;
	char *s;
	__u8 *out;
	int ret;

	__CPROVER_assume(len <= 2 * LEN_MAX + 1 && n <= LEN_MAX);
	s = any_bytes(len + 1);
	__CPROVER_assume(!s[len] && strnlen(s, len) == len);
	out = any_bytes(n);
	ret = hex_decode(s, out, n);
	for (i = 0; i < len; i++)
		if (digit(s[i]) < 0)
			hex = false;
	__CPROVER_assert(!ret || ret == -EINVAL, "0 or -EINVAL");
	__CPROVER_assert(!ret == (len == 2 * n && hex),
			 "exactly 2 * n hex digits");
	for (i = 0; !ret && i < n; i++)
		__CPROVER_assert(out[i] == digit(s[2 * i]) * 16 +
					    digit(s[2 * i + 1]),
				 "the value of each pair");
}

void harness(void)
{
	encoded();
	decoded();
}
