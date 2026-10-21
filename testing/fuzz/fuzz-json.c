// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * json_parse() over a schema with every field type.
 */

#include "fuzz.h"
#include "json.h"
#include "macros.h"

static int on_elem(void *ctx)
{
	(*(int *)ctx)++;
	return 0;
}

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	char s[64], k[32], v[16];
	__s64 i = 0, n = 0;
	int b = 0, elems = 0;
	struct shirudo_json_array arr = { on_elem, &elems };
	struct shirudo_json_field obj[] = {
		{ "k", SHIRUDO_JSON_T_STRING, k, sizeof(k), 0, NULL, 0 },
		{ "n", SHIRUDO_JSON_T_INT, &n, 0, 0, NULL, 0 },
	};
	struct shirudo_json_field elem[] = {
		{ "v", SHIRUDO_JSON_T_STRING, v, sizeof(v), 0, NULL, 0 },
		{ "o", SHIRUDO_JSON_T_OBJECT, NULL, 0, 0, obj, ARRAY_SIZE(obj) },
	};
	struct shirudo_json_field root[] = {
		{ "s", SHIRUDO_JSON_T_STRING, s, sizeof(s), 0, NULL, 0 },
		{ "i", SHIRUDO_JSON_T_INT, &i, 0, 0, NULL, 0 },
		{ "b", SHIRUDO_JSON_T_BOOL, &b, 0, 0, NULL, 0 },
		{ "o", SHIRUDO_JSON_T_OBJECT, NULL, 0, 0, obj, ARRAY_SIZE(obj) },
		{ "a", SHIRUDO_JSON_T_ARRAY, &arr, 0, 0, elem, ARRAY_SIZE(elem) },
	};

	json_parse((const char *)data, size, root, ARRAY_SIZE(root));
	return 0;
}
