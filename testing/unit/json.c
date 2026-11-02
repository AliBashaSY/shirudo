// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <stdint.h>

#include <shirudo/string.h>

#include "json.h"
#include "unit.h"

/* What an eventlog-shaped schema extracts from one document. */
struct doc {
	char digest[65];
	char type[64];
	__s64 schema;
	int names;
	char first_name[64];
	char name[64];
	int events;
	char last_type[32];
	char ev_type[32];
	char ev_digest[65];
};

static int name_on_elem(void *ctx)
{
	struct doc *d = ctx;

	if (!d->names++)
		strscpy(d->first_name, d->name, sizeof(d->first_name));
	return 0;
}

static int event_on_elem(void *ctx)
{
	struct doc *d = ctx;

	strscpy(d->last_type, d->ev_type, sizeof(d->last_type));
	d->events++;
	return 0;
}

static int parse_len(const char *text, size_t len, struct doc *d)
{
	struct json_array event_sink = { event_on_elem, d };
	struct json_array name_sink = { name_on_elem, d };
	struct json_field name_sub[] = {
		{ "", JSON_T_STRING, d->name, sizeof(d->name),
		  0, NULL, 0 },
	};
	struct json_field event_sub[] = {
		{ "type", JSON_T_STRING, d->ev_type,
		  sizeof(d->ev_type), 0, NULL, 0 },
		{ "digest", JSON_T_STRING, d->ev_digest,
		  sizeof(d->ev_digest), 0, NULL, 0 },
	};
	struct json_field f[] = {
		{ "schema", JSON_T_INT, &d->schema, 0, 0, NULL, 0 },
		{ "type", JSON_T_STRING, d->type, sizeof(d->type),
		  0, NULL, 0 },
		{ "digest", JSON_T_STRING, d->digest,
		  sizeof(d->digest), 0, NULL, 0 },
		{ "names", JSON_T_ARRAY, &name_sink, 0, 0,
		  name_sub, ARRAY_SIZE(name_sub) },
		{ "events", JSON_T_ARRAY, &event_sink, 0, 0,
		  event_sub, ARRAY_SIZE(event_sub) },
		/* An empty schema slot must be ignored, not dereferenced. */
		{ NULL, 0, NULL, 0, 0, NULL, 0 },
	};

	memset(d, 0, sizeof(*d));
	d->schema = -1;
	return json_parse(text, len, f, ARRAY_SIZE(f));
}

static int parse(const char *text, struct doc *d)
{
	return parse_len(text, strlen(text), d);
}

static void extracts_scalars_and_skips_nested_sets(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"schema\":1,\"digest\":\"abcdef\",\"type\":\"init\","
			"\"set\":[{\"name\":\"x\"}]}", &d), 0);
	EXPECT_STREQ(d.digest, "abcdef");
	EXPECT_STREQ(d.type, "init");
	EXPECT_EQ(d.schema, 1);
}

static void skips_unknown_keys_and_objects(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"a\":{\"b\":[1,2,3]},\"digest\":\"ff\",\"c\":true}",
			&d), 0);
	EXPECT_STREQ(d.digest, "ff");
	ASSERT_EQ(parse("{\"unknown\":true,\"digest\":\"ff\"}", &d), 0);
	EXPECT_STREQ(d.digest, "ff");
}

static void discarded_key_does_not_dispatch(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"a\":{\"digest\":123,\"type\":true},"
			"\"digest\":\"ff\",\"type\":\"init\"}", &d), 0);
	EXPECT_STREQ(d.digest, "ff");
	EXPECT_STREQ(d.type, "init");
}

/*
 * A value read as a C string would end at the NUL and pass checks meant
 * for the whole value; a key would be dispatched as its prefix.
 */
static void escaped_nul_is_refused(void)
{
	struct doc d;

	EXPECT_EQ(parse("{\"digest\":\"ab\\u0000cd\"}", &d), -EINVAL);
	EXPECT_EQ(parse("{\"digest\\u0000x\":\"ff\"}", &d), -EINVAL);
	ASSERT_EQ(parse("{\"digest\":\"ab\\u0041\"}", &d), 0);
	EXPECT_STREQ(d.digest, "abA");
}

static void malformed_documents_are_refused(void)
{
	struct doc d;

	EXPECT_EQ(parse("{\"digest\":\"abc", &d), -EINVAL);
	EXPECT_EQ(parse("{\"digest\":\"a\"}xyz", &d), -EINVAL);
	EXPECT_EQ(parse("{\"digest\":123}", &d), -EINVAL);
}

static void overlong_string_is_enospc(void)
{
	char text[256], big[201];
	struct doc d;

	memset(big, '0', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	ASSERT_TRUE(strsprintf(text, sizeof(text), "{\"digest\":\"%s\"}",
			       big) > 0);
	EXPECT_EQ(parse(text, &d), -ENOSPC);
}

static void deep_nesting_is_e2big(void)
{
	char text[256];
	size_t n = 0;
	struct doc d;

	for (int i = 0; i < 40; i++) {
		memcpy(text + n, "{\"a\":", 5);
		n += 5;
	}
	text[n++] = '1';
	memset(text + n, '}', 40);
	n += 40;
	EXPECT_EQ(parse_len(text, n, &d), -E2BIG);
}

static void garbage_fails_cleanly(void)
{
	static char buf[4096];
	__u64 x = 0x9e3779b97f4a7c15ULL;
	struct doc d;

	for (int r = 0; r < 64; r++) {
		for (size_t i = 0; i < sizeof(buf); i++) {
			x ^= x << 13;
			x ^= x >> 7;
			x ^= x << 17;
			buf[i] = (char)x;
		}
		EXPECT_TRUE(parse_len(buf, sizeof(buf), &d) <= 0);
	}
}

static void empty_object_parses(void)
{
	struct doc d;

	EXPECT_EQ(parse("{}", &d), 0);
}

static void string_array_dispatches_each_element(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"names\":[\"alpha\",\"beta\",\"gamma\"]}", &d), 0);
	EXPECT_EQ(d.names, 3);
	EXPECT_STREQ(d.first_name, "alpha");
	ASSERT_EQ(parse("{\"names\":[]}", &d), 0);
	EXPECT_EQ(d.names, 0);
}

static void object_array_uses_element_schema(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"events\":[{\"type\":\"init\",\"digest\":\"00\"},"
			"{\"type\":\"upatch_load\",\"digest\":\"ff\"}]}", &d), 0);
	EXPECT_EQ(d.events, 2);
	EXPECT_STREQ(d.last_type, "upatch_load");
}

static void nested_array_then_trailing_scalar(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"events\":[{\"type\":\"init\",\"digest\":\"aa\","
			"\"set\":[{\"name\":\"x\"}]}],\"digest\":\"top\"}", &d), 0);
	EXPECT_STREQ(d.digest, "top");
	EXPECT_EQ(d.events, 1);
	EXPECT_STREQ(d.last_type, "init");
}

static void malformed_arrays_are_refused(void)
{
	struct doc d;

	EXPECT_EQ(parse("{\"names\":[123]}", &d), -EINVAL);
	EXPECT_EQ(parse("{\"names\":[\"a\",\"b\"", &d), -EINVAL);
}

static void integer_takes_json_integers_only(void)
{
	static const char *const bad[] = {
		"1.9", "1e9", "1E9", "7-3", "1+1", "-", ".", "01", "-01", "1.",
		".5", "1e", "--1", "0x1", "+1",
	};
	char text[64];
	struct doc d;

	for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
		ASSERT_TRUE(strsprintf(text, sizeof(text), "{\"schema\":%s}",
				       bad[i]) > 0);
		EXPECT_EQ(parse(text, &d), -EINVAL);
	}
}

static void integer_holds_any_s64_and_no_more(void)
{
	struct doc d;

	ASSERT_EQ(parse("{\"schema\":-9223372036854775808}", &d), 0);
	EXPECT_EQ(d.schema, INT64_MIN);
	ASSERT_EQ(parse("{\"schema\":9223372036854775807}", &d), 0);
	EXPECT_EQ(d.schema, INT64_MAX);
	ASSERT_EQ(parse("{\"schema\":-0}", &d), 0);
	EXPECT_EQ(d.schema, 0);
	EXPECT_EQ(parse("{\"schema\":9223372036854775808}", &d), -EINVAL);
	EXPECT_EQ(parse("{\"schema\":-9223372036854775809}", &d), -EINVAL);
}

static void discarded_number_follows_the_grammar(void)
{
	static const char *const bad[] = { "1.", "01", "1e+", "-.5", "1.e5" };
	char text[64];
	struct doc d;

	ASSERT_EQ(parse("{\"x\":[1.5e+10,-0.0E-1,2e5,0],\"schema\":2}", &d), 0);
	EXPECT_EQ(d.schema, 2);
	for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
		ASSERT_TRUE(strsprintf(text, sizeof(text),
				       "{\"x\":%s,\"schema\":2}", bad[i]) > 0);
		EXPECT_EQ(parse(text, &d), -EINVAL);
	}
}

static const struct unit_case cases[] = {
	UNIT_CASE(extracts_scalars_and_skips_nested_sets),
	UNIT_CASE(skips_unknown_keys_and_objects),
	UNIT_CASE(discarded_key_does_not_dispatch),
	UNIT_CASE(escaped_nul_is_refused),
	UNIT_CASE(malformed_documents_are_refused),
	UNIT_CASE(overlong_string_is_enospc),
	UNIT_CASE(deep_nesting_is_e2big),
	UNIT_CASE(garbage_fails_cleanly),
	UNIT_CASE(empty_object_parses),
	UNIT_CASE(string_array_dispatches_each_element),
	UNIT_CASE(object_array_uses_element_schema),
	UNIT_CASE(nested_array_then_trailing_scalar),
	UNIT_CASE(malformed_arrays_are_refused),
	UNIT_CASE(integer_takes_json_integers_only),
	UNIT_CASE(integer_holds_any_s64_and_no_more),
	UNIT_CASE(discarded_number_follows_the_grammar),
	{}
};

UNIT_SUITE(json, cases);
