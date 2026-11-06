// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * dm_name_ok() and dm_device() of util/dm_verity.c on every string of up to
 * LEN_MAX bytes: a device-mapper name is 1 to 127 of [A-Za-z0-9._-] and not
 * "." or "..", and a device is two decimal numbers around a ':' that the
 * device number holds unchanged.
 */

#define PROOF_DEVICE_NUMBERS
#include "proof.h"
#include "../../../util/string.c"
#include "../../../util/dm_verity.c"

#define LEN_MAX	7

static char *any_string(void)
{
	char *s = any_bytes(LEN_MAX + 1);
	size_t len;

	__CPROVER_assume(len <= LEN_MAX);
	s[LEN_MAX] = '\0';
	return s + LEN_MAX - len;
}

static bool name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static bool decimal(const char *s, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (s[i] < '0' || s[i] > '9')
			return false;
	return n;
}

void harness(void)
{
	char *name = any_string(), *token = any_string(), *was;
	size_t n = strlen(name), i, colon;
	dev_t dev;

	if (dm_name_ok(name)) {
		__CPROVER_assert(n && strcmp(name, ".") && strcmp(name, ".."),
				 "a name, not . or ..");
		for (i = 0; i < n; i++)
			__CPROVER_assert(name_char(name[i]), "of [A-Za-z0-9._-]");
	}
	n = strlen(token);
	was = any_bytes(n + 1);
	memcpy(was, token, n + 1);
	if (dm_device(token, &dev))
		return;
	colon = strchr(was, ':') - was;
	__CPROVER_assert(decimal(was, colon) &&
			 decimal(was + colon + 1, n - colon - 1),
			 "decimal major:minor");
}
