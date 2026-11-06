// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * bpm_header() of util/bpm_oci.c, libcurl's header callback, for every
 * header line of up to LINE_MAX bytes, NULs included: it takes the whole
 * line, and the challenge, media type and location it keeps are terminated
 * inside their fields, the media type without parameters, neither with a
 * line break.
 */

/* util/string.c is included behind a file that includes its header. */
#define STRING_INTERNAL
#define PROOF_MEMCPY_BYTEWISE

#include "proof.h"
#include "../../../util/bpm_oci.c"
#include "../../../util/string.c"

#define LINE_MAX	20

/* What the line leaves in a field ends inside the line's length. */
static bool short_string(const char *s)
{
	return memchr(s, '\0', LINE_MAX + 1);
}

static bool has_any(const char *s, const char *set)
{
	size_t i, j;

	for (i = 0; s[i]; i++)
		for (j = 0; set[j]; j++)
			if (s[i] == set[j])
				return true;
	return false;
}

void harness(void)
{
	char *line = any_bytes(LINE_MAX);
	struct bpm_http_response r = {};
	size_t n;

	__CPROVER_assume(n <= LINE_MAX);
	__CPROVER_assert(bpm_header(line, 1, n, &r) == n,
			 "the whole line taken");
	__CPROVER_assert(short_string(r.www_auth) &&
			 short_string(r.content_type) &&
			 short_string(r.location),
			 "every value terminated inside its field");
	__CPROVER_assert(!has_any(r.content_type, " ;\r\n"),
			 "a media type alone");
	__CPROVER_assert(!has_any(r.location, "\r\n"),
			 "a location without its line break");
	__CPROVER_assert(!isspace((__u8)r.www_auth[0]) &&
			 !isspace((__u8)r.content_type[0]) &&
			 !isspace((__u8)r.location[0]),
			 "no value starts with a blank");
}
