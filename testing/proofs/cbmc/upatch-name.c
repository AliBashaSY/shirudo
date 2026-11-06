// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The upatch names util/upatch.c takes from outside, for every name of up
 * to LEN_MAX bytes: upatch_name_valid() accepts exactly [A-Za-z0-9._-]+
 * not starting with a dot, so no path separator, "." or "..", and
 * upatch_select() exactly the archive names, something then ".upatch".
 */

#include "proof.h"
#include "../../../util/string.c"
#include "../../../util/upatch.c"

#define LEN_MAX	24

static bool name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static void name_valid(void)
{
	bool valid, chars = true;
	size_t n, i;
	char *name;

	__CPROVER_assume(n <= LEN_MAX);
	name = any_bytes(n + 1);
	__CPROVER_assume(!name[n] && strnlen(name, n) == n);
	valid = upatch_name_valid(name);
	for (i = 0; i < n; i++)
		chars &= name_char(name[i]);
	__CPROVER_assert(valid == (n > 0 && name[0] != '.' && chars),
			 "exactly the names of the pattern");
	for (i = 0; i < n; i++)
		__CPROVER_assert(!valid || name[i] != '/', "no separator");
	__CPROVER_assert(!upatch_name_valid(NULL), "no name is invalid");
}

static void archive_name(void)
{
	static const char suffix[] = ".upatch";
	const size_t suffix_len = sizeof(suffix) - 1;
	struct dirent e;
	size_t n;
	int ret;

	__CPROVER_assume(n <= LEN_MAX + suffix_len &&
			 !e.d_name[n] && strnlen(e.d_name, n) == n);
	ret = upatch_select(&e);
	__CPROVER_assert(ret == (n > suffix_len &&
				 !memcmp(e.d_name + n - suffix_len, suffix,
					 suffix_len)),
			 "exactly a name before .upatch");
}

void harness(void)
{
	name_valid();
	archive_name();
}
