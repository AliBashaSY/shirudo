// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The bpm source and tree config lines util/bpm_source_config.c reads, for
 * every line of up to LINE_MAX bytes: a setting is written only for its own
 * key and terminated inside its buffer, a source one led by no blank and
 * without a line break, a tree one unquoted without blanks around it.
 */

/* util/string.c is included behind a file that includes its header. */
#define STRING_INTERNAL

#include "proof.h"
#include "../../../util/bpm_source_config.c"
#include "../../../util/string.c"

#define LINE_MAX	12
#define OUT		4

static bool blank(char c)
{
	return c == ' ' || (c >= '\t' && c <= '\r');
}

static bool has(const char *s, char c)
{
	return memchr(s, c, strnlen(s, OUT));
}

/* Written for its key alone, and then a terminated value. */
static bool setting(const char *out, const char *was, bool mine, bool lead,
		    bool trail)
{
	size_t n;

	if (!mine)
		return !memcmp(out, was, OUT);
	n = strnlen(out, OUT);
	return n < OUT && (!n || ((!lead || !blank(out[0])) &&
				  (!trail || !blank(out[n - 1]))));
}

/* A value that starts with a quote, which keeps the blanks inside it. */
static bool quoted(const char *eq)
{
	if (!eq)
		return false;
	for (eq++; blank(*eq); eq++)
		;
	return *eq == '"';
}

/* The key a parsed line leaves at its start, the blanks around it gone. */
static bool key_is(const char *line, const char *eq, const char *name)
{
	while (blank(*line))
		line++;
	return eq && !strcmp(line, name);
}

static char *any_line(void)
{
	char *line = any_bytes(LINE_MAX + 1);

	__CPROVER_assume(!line[LINE_MAX]);
	return line;
}

static void source_line(void)
{
	char endpoint[OUT], repo[OUT], key[OUT], e[OUT], r[OUT], k[OUT];
	char *line = any_line(), *eq = strchr(line, '=');
	bool is_e, is_r, is_k;

	memcpy(e, endpoint, OUT);
	memcpy(r, repo, OUT);
	memcpy(k, key, OUT);
	if (bpm_parse_source_line(line, endpoint, OUT, repo, OUT, key, OUT))
		return;
	is_e = key_is(line, eq, "endpoint");
	is_r = key_is(line, eq, "index_repository");
	is_k = key_is(line, eq, "key_file");
	__CPROVER_assert(setting(endpoint, e, is_e, true, false) &&
			 setting(repo, r, is_r, true, false) &&
			 setting(key, k, is_k, true, false),
			 "a setting for its key, led by no blank");
	__CPROVER_assert((!is_e || (!has(endpoint, '\r') &&
				    !has(endpoint, '\n'))) &&
			 (!is_r || (!has(repo, '\r') && !has(repo, '\n'))) &&
			 (!is_k || (!has(key, '\r') && !has(key, '\n'))),
			 "no line break in a setting");
}

static void tree_line(void)
{
	char endpoint[OUT], reference[OUT], e[OUT], r[OUT];
	char *line = any_line(), *eq = strchr(line, '=');
	bool is_e, is_r, bare = !quoted(eq);
	int ret;

	memcpy(e, endpoint, OUT);
	memcpy(r, reference, OUT);
	ret = bpm_parse_tree_line(line, endpoint, OUT, reference, OUT);
	if (ret == -EINVAL) {
		__CPROVER_assert(eq, "a refused line has a value");
		return;
	}
	if (ret)
		return;
	is_e = key_is(line, eq, "endpoint");
	is_r = key_is(line, eq, "reference");
	__CPROVER_assert(setting(endpoint, e, is_e, bare, bare) &&
			 setting(reference, r, is_r, bare, bare),
			 "a setting for its key, unquoted without blanks around");
}

void harness(void)
{
	source_line();
	tree_line();
}
