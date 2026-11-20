// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * token_from_body() of util/bpm_oci.c, on a token server's answer of up to
 * TEXT_MAX bytes: the token it takes is a quoted value of an answer that
 * names one, terminated and fit for a header line, and a refused answer
 * writes no token. And token_from_reply(), on the response buffer curl
 * filled with up to TEXT_MAX bytes of any value: an answer without a
 * body or with a NUL among its bytes gives no token, and any other gives
 * what token_from_body() takes from all of its bytes.
 */

/* util/string.c is included behind a file that includes its header. */
#define STRING_INTERNAL

#include "proof.h"
#include "../../../util/bpm_oci.c"
#include "../../../util/string.c"

#define TEXT_MAX	17
#define OUT_MAX		3

static char *text(void)
{
	char *s = any_bytes(TEXT_MAX + 1);

	__CPROVER_assume(!s[TEXT_MAX]);
	return s;
}

static bool terminated(const char *s, size_t size)
{
	return memchr(s, '\0', size);
}

static bool header_safe(const char *s)
{
	for (; *s; s++)
		if ((__u8)*s < 0x21 || *s == '\\' || *s == '"' ||
		    (__u8)*s == 0x7f)
			return false;
	return true;
}

static bool quoted_in(const char *s, size_t len, const char *value)
{
	size_t v = strlen(value), i, j;
	bool match;

	for (i = 0; i + v + 2 <= len; i++) {
		match = s[i] == '"' && s[i + v + 1] == '"';
		for (j = 0; match && j < v; j++)
			match = s[i + 1 + j] == value[j];
		if (match)
			return true;
	}
	return false;
}

static void answer_text(void)
{
	char out[OUT_MAX], was[OUT_MAX];
	char *body = text();
	size_t len = strlen(body);

	memcpy(was, out, sizeof(out));
	if (token_from_body(body, out, sizeof(out))) {
		__CPROVER_assert(!memcmp(out, was, sizeof(out)),
				 "a refused answer writes no token");
		return;
	}
	__CPROVER_assert(terminated(out, sizeof(out)), "a terminated token");
	__CPROVER_assert(header_safe(out),
			 "no blank, control, quote or backslash in the token");
	__CPROVER_assert(quoted_in(body, len, out) &&
			 (str_contains(body, "\"token\"") ||
			  str_contains(body, "\"access_token\"")),
			 "a quoted value of an answer naming a token");
}

/* The response as bpm_buffer_append() leaves it: a NUL behind its bytes. */
static void answer_bytes(void)
{
	char out[OUT_MAX], was[OUT_MAX], same[OUT_MAX];
	struct bpm_buffer reply = { .cap = TEXT_MAX + 1 }, none = {};
	char *body = any_bytes(TEXT_MAX + 1);
	bool nul = false;
	size_t len, i;
	int ret;

	__CPROVER_assume(len <= TEXT_MAX && !body[len]);
	reply.data = (__u8 *)body;
	reply.len = len;
	for (i = 0; i < len; i++)
		if (!body[i])
			nul = true;
	memcpy(was, out, sizeof(out));
	memcpy(same, out, sizeof(out));
	__CPROVER_assert(token_from_reply(&none, out, sizeof(out)) == -EINVAL &&
			 !memcmp(out, was, sizeof(out)),
			 "no token from an answer without a body");
	ret = token_from_reply(&reply, out, sizeof(out));
	if (nul) {
		__CPROVER_assert(ret == -EINVAL &&
				 !memcmp(out, was, sizeof(out)),
				 "no token from an answer with a NUL among its bytes");
		return;
	}
	__CPROVER_assert(ret == token_from_body(body, same, sizeof(same)) &&
			 !memcmp(out, same, sizeof(out)),
			 "else the token of the answer's text, or none");
}

void harness(void)
{
	answer_text();
	answer_bytes();
}
