// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <shirudo/misc.h>

#include "bpm_source_config.h"

int shirudo_bpm_parse_source_line(char *line,
				  char *endpoint, size_t endpoint_sz,
				  char *key_file, size_t key_file_sz)
{
	char *eq, *key, *key_end, *val;

	if (!line || !endpoint || !endpoint_sz || !key_file || !key_file_sz)
		return -EINVAL;

	key = line;
	while (isspace((__u8)*key))
		key++;
	eq = strchr(key, '=');
	if (!eq)
		return 0;
	*eq++ = '\0';
	key_end = eq - 1;
	while (key_end > key && isspace((__u8)key_end[-1]))
		*--key_end = '\0';

	val = eq;
	while (isspace((__u8)*val))
		val++;
	val[strcspn(val, "\r\n")] = '\0';

	if (!strcmp(key, "endpoint"))
		snprintf(endpoint, endpoint_sz, "%s", val);
	else if (!strcmp(key, "key_file"))
		snprintf(key_file, key_file_sz, "%s", val);
	return 0;
}

static char *trim(char *s)
{
	char *end;

	while (isspace((__u8)*s))
		s++;
	end = s + strlen(s);
	while (end > s && isspace((__u8)end[-1]))
		end--;
	*end = '\0';
	return s;
}

int shirudo_bpm_parse_tree_line(char *line,
				char *endpoint, size_t endpoint_sz,
				char *reference, size_t reference_sz)
{
	char *eq = strchr(line, '=');
	char *key, *value;
	size_t len;

	if (!eq)
		return 0;
	*eq++ = '\0';
	key = trim(line);
	value = trim(eq);
	len = strlen(value);
	if (value[0] == '"') {
		if (len < 2 || value[len - 1] != '"')
			return -EINVAL;
		value[len - 1] = '\0';
		value++;
	}
	if (!strcmp(key, "endpoint"))
		snprintf(endpoint, endpoint_sz, "%s", value);
	else if (!strcmp(key, "reference"))
		snprintf(reference, reference_sz, "%s", value);
	return 0;
}
