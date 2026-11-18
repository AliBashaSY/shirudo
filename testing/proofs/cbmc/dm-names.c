// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * dm_name_ok() and dm_device() of util/dm_verity.c on every string of up to
 * LEN_MAX bytes, and on names of 127 and 128 bytes that end in letters: a
 * device-mapper name is exactly 1 to 127 of [A-Za-z0-9._-] and not "." or
 * "..", and a device is two decimal numbers around a ':' that the device
 * number holds unchanged.
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

#define DM_NAME_LEN	127

static bool name_valid(const char *name, size_t n)
{
	bool chars = true;
	size_t i;

	for (i = 0; i < n; i++)
		chars = chars && name_char(name[i]);
	return n && n <= DM_NAME_LEN && chars && strcmp(name, ".") &&
	       strcmp(name, "..");
}

/* Any LEN_MAX bytes first, letters up to len after them. */
static void long_name(size_t len)
{
	char *name = any_bytes(len + 1);
	size_t i;

	for (i = 0; i < LEN_MAX; i++)
		__CPROVER_assume(name[i]);
	for (i = LEN_MAX; i < len; i++)
		name[i] = 'a';
	name[len] = '\0';
	__CPROVER_assert(dm_name_ok(name) == name_valid(name, len),
			 "a long name up to 127 bytes");
}

void harness(void)
{
	char *name = any_string(), *token = any_string(), *was;
	size_t n = strlen(name), colon;
	dev_t dev;

	__CPROVER_assert(dm_name_ok(name) == name_valid(name, n),
			 "1 to 127 of [A-Za-z0-9._-], not . or ..");
	long_name(DM_NAME_LEN);
	long_name(DM_NAME_LEN + 1);
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
