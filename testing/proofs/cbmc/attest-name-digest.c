// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * attest_parse_name_digest() of util/attest.c, which reads attest
 * predict's --expect and --event values, on every string of LEN bytes:
 * accepted exactly when a name of one byte or more runs up to the first
 * equals sign and 64 hex digits follow it, into that name and the value
 * of those digits.
 */

#include <stdbool.h>

#include "proof.h"
#include "../../../util/string.c"
#include "../../../util/hex.c"
#include "../../../util/attest.c"

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

void harness(void)
{
	char name[UPATCH_NAME_MAX + 1];
	bool hex = true, ok;
	size_t eq = LEN, i;
	__u8 digest[32];
	char *s;
	int ret;

	s = any_bytes(LEN + 1);
	__CPROVER_assume(!s[LEN] && strnlen(s, LEN) == LEN);
	ret = attest_parse_name_digest(s, name, digest);
	for (i = LEN; i > 0; i--)
		if (s[i - 1] == '=')
			eq = i - 1;
	for (i = eq + 1; i < LEN; i++)
		if (digit(s[i]) < 0)
			hex = false;
	ok = eq < LEN && eq >= 1 && LEN - eq - 1 == 64 && hex;
	__CPROVER_assert(!ret == ok,
			 "a name up to the first equals sign, then 64 digits");
	if (ret)
		return;
	__CPROVER_assert(strnlen(name, sizeof(name)) == eq &&
			 !memcmp(name, s, eq), "the name");
	for (i = 0; i < 32; i++)
		__CPROVER_assert(digest[i] == digit(s[eq + 1 + 2 * i]) * 16 +
					      digit(s[eq + 2 + 2 * i]),
				 "the value of the digits");
}
