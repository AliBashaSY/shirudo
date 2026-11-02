// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <stdint.h>

#include <shirudo/string.h>

#include "malloc.h"
#include "unit.h"

/*
 * The span under test sits in a buffer of exactly its length, so a read
 * past it shows up under ASan.
 */
static char *span(const char *text, size_t len)
{
	char *buf = xmalloc(len ? len : 1);

	ASSERT_TRUE(buf);
	memcpy(buf, text, len);
	return buf;
}

static int u64_of(const char *text, size_t len, __u64 max, __u64 *v)
{
	char *buf = span(text, len);
	int ret;

	*v = 0;
	ret = parse_u64(buf, len, max, v);
	xfree(buf, len ? len : 1);
	return ret;
}

static int x64_of(const char *text, __u64 max, __u64 *v)
{
	size_t len = strlen(text);
	char *buf = span(text, len);
	int ret;

	*v = 0;
	ret = parse_x64(buf, len, max, v);
	xfree(buf, len ? len : 1);
	return ret;
}

static int s64_of(const char *text, __s64 min, __s64 max, __s64 *v)
{
	size_t len = strlen(text);
	char *buf = span(text, len);
	int ret;

	*v = 0;
	ret = parse_s64(buf, len, min, max, v);
	xfree(buf, len ? len : 1);
	return ret;
}

static int u64_str(const char *text, __u64 max, __u64 *v)
{
	return u64_of(text, strlen(text), max, v);
}

#define EXPECT_U64(text, max, ret, value)				\
	do {								\
		__u64 v_;						\
									\
		EXPECT_EQ(u64_str(text, max, &v_), ret);			\
		EXPECT_UEQ(v_, value);					\
	} while (0)

static void digits_counts_leading_digits(void)
{
	EXPECT_UEQ(digits("123abc", 6), 3);
	EXPECT_UEQ(digits("abc", 3), 0);
	EXPECT_UEQ(digits("", 0), 0);
}

static void decimal_fills_its_span(void)
{
	EXPECT_U64("0", UINT64_MAX, 0, 0);
	EXPECT_U64("007", UINT64_MAX, 0, 7);
	EXPECT_U64("18446744073709551615", UINT64_MAX, 0, UINT64_MAX);
}

static void nothing_past_the_span_is_read(void)
{
	__u64 v;

	EXPECT_EQ(u64_of("123456", 3, UINT64_MAX, &v), 0);
	EXPECT_UEQ(v, 123);
	EXPECT_EQ(u64_of("123", 0, UINT64_MAX, &v), -EINVAL);
	EXPECT_UEQ(v, 0);
}

static void sign_blank_or_prefix_is_no_number(void)
{
	static const char *const bad[] = {
		"", "-1", "+1", " 1", "1 ", "0x10", "1.5", "1e3", "12a",
	};

	for (size_t i = 0; i < ARRAY_SIZE(bad); i++)
		EXPECT_U64(bad[i], UINT64_MAX, -EINVAL, 0);
}

static void number_past_its_bound_is_out_of_range(void)
{
	EXPECT_U64("18446744073709551616", UINT64_MAX, -ERANGE, 0);
	EXPECT_U64("10", 10, 0, 10);
	EXPECT_U64("11", 10, -ERANGE, 0);
	EXPECT_U64("9", 5, -ERANGE, 0);
	EXPECT_U64("0", 0, 0, 0);
}

static void malformed_number_however_long(void)
{
	EXPECT_U64("99999999999999999999x", UINT64_MAX, -EINVAL, 0);
}

static void hex_takes_either_case_and_no_prefix(void)
{
	__u64 v;

	EXPECT_EQ(x64_of("ff", UINT64_MAX, &v), 0);
	EXPECT_UEQ(v, 255);
	EXPECT_EQ(x64_of("FF", UINT64_MAX, &v), 0);
	EXPECT_UEQ(v, 255);
	EXPECT_EQ(x64_of("ffffffffffffffff", UINT64_MAX, &v), 0);
	EXPECT_UEQ(v, UINT64_MAX);
	EXPECT_EQ(x64_of("10000000000000000", UINT64_MAX, &v), -ERANGE);
	EXPECT_UEQ(v, 0);
	EXPECT_EQ(x64_of("0x10", UINT64_MAX, &v), -EINVAL);
	EXPECT_UEQ(v, 0);
	EXPECT_EQ(x64_of("g", UINT64_MAX, &v), -EINVAL);
	EXPECT_UEQ(v, 0);
}

static void signed_number_spans_the_whole_range(void)
{
	__s64 v;

	EXPECT_EQ(s64_of("-9223372036854775808", INT64_MIN, INT64_MAX, &v), 0);
	EXPECT_EQ(v, INT64_MIN);
	EXPECT_EQ(s64_of("9223372036854775807", INT64_MIN, INT64_MAX, &v), 0);
	EXPECT_EQ(v, INT64_MAX);
	EXPECT_EQ(s64_of("-0", INT64_MIN, INT64_MAX, &v), 0);
	EXPECT_EQ(v, 0);
	EXPECT_EQ(s64_of("9223372036854775808", INT64_MIN, INT64_MAX, &v),
		  -ERANGE);
	EXPECT_EQ(v, 0);
	EXPECT_EQ(s64_of("-9223372036854775809", INT64_MIN, INT64_MAX, &v),
		  -ERANGE);
	EXPECT_EQ(v, 0);
}

static void signed_number_keeps_bounds_and_form(void)
{
	static const char *const bad[] = { "-", "--", "--1", "+1", " -1", "-x" };
	__s64 v;

	EXPECT_EQ(s64_of("0", 1, 10, &v), -ERANGE);
	EXPECT_EQ(v, 0);
	EXPECT_EQ(s64_of("-6", -5, 5, &v), -ERANGE);
	EXPECT_EQ(v, 0);
	EXPECT_EQ(s64_of("-5", -5, 5, &v), 0);
	EXPECT_EQ(v, -5);
	for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
		EXPECT_EQ(s64_of(bad[i], INT64_MIN, INT64_MAX, &v), -EINVAL);
		EXPECT_EQ(v, 0);
	}
}

static void strscpy_copies_what_fits_whole(void)
{
	char six[6], big[64], one[1];

	EXPECT_EQ(strscpy(six, "hello", sizeof(six)), 5);
	EXPECT_STREQ(six, "hello");
	EXPECT_EQ(strscpy(big, "hello", sizeof(big)), 5);
	EXPECT_STREQ(big, "hello");
	EXPECT_EQ(strscpy(one, "", sizeof(one)), 0);
	EXPECT_STREQ(one, "");
}

static void strscpy_cuts_and_says_so(void)
{
	char five[5], one[1], none[1] = { 'x' };

	EXPECT_EQ(strscpy(five, "hello", sizeof(five)), -E2BIG);
	EXPECT_STREQ(five, "hell");
	EXPECT_EQ(strscpy(one, "hello", sizeof(one)), -E2BIG);
	EXPECT_STREQ(one, "");
	EXPECT_EQ(strscpy(none, "hello", 0), -E2BIG);
	EXPECT_EQ(none[0], 'x');
}

static void strsprintf_writes_what_fits_whole(void)
{
	char eight[8], big[64];

	EXPECT_EQ(strsprintf(eight, sizeof(eight), "<%s>", "hello"), 7);
	EXPECT_STREQ(eight, "<hello>");
	EXPECT_EQ(strsprintf(big, sizeof(big), "<%s>", ""), 2);
	EXPECT_STREQ(big, "<>");
}

static void strsprintf_leaves_nothing_when_cut(void)
{
	char seven[7] = { 'x' }, one[1] = { 'x' }, none[1] = { 'x' };

	EXPECT_EQ(strsprintf(seven, sizeof(seven), "<%s>", "hello"), -E2BIG);
	EXPECT_STREQ(seven, "");
	EXPECT_EQ(strsprintf(one, sizeof(one), "<%s>", "hello"), -E2BIG);
	EXPECT_STREQ(one, "");
	EXPECT_EQ(strsprintf(none, 0, "<%s>", "hello"), -E2BIG);
	EXPECT_EQ(none[0], 'x');
}

static void xasprintf_allocates_the_size_needed(void)
{
	char text[301], want[310], *s;

	s = xasprintf("<%s|%d>", "hello", 42);
	ASSERT_TRUE(s);
	EXPECT_STREQ(s, "<hello|42>");
	xstrfree(s);

	s = xasprintf("<%s|%d>", "", 42);
	ASSERT_TRUE(s);
	EXPECT_STREQ(s, "<|42>");
	xstrfree(s);

	memset(text, '0', sizeof(text) - 1);
	text[sizeof(text) - 1] = '\0';
	ASSERT_EQ(strsprintf(want, sizeof(want), "<%s|42>", text), 305);
	s = xasprintf("<%s|%d>", text, 42);
	ASSERT_TRUE(s);
	EXPECT_STREQ(s, want);
	xstrfree(s);
}

static void dns_name_is_ldh_labels_and_no_address(void)
{
	char label[65], name[300];

	EXPECT_TRUE(dns_name_valid("registry.example.com"));
	EXPECT_TRUE(dns_name_valid("Reg-1.Example"));
	EXPECT_TRUE(dns_name_valid("localhost"));
	EXPECT_TRUE(dns_name_valid("1.2.3.example"));
	EXPECT_TRUE(dns_name_valid("0x.1"));
	EXPECT_FALSE(dns_name_valid(""));
	EXPECT_FALSE(dns_name_valid("."));
	EXPECT_FALSE(dns_name_valid("example..com"));
	EXPECT_FALSE(dns_name_valid("example.com."));
	EXPECT_FALSE(dns_name_valid("-example.com"));
	EXPECT_FALSE(dns_name_valid("example-.com"));
	EXPECT_FALSE(dns_name_valid("exa_mple.com"));
	EXPECT_FALSE(dns_name_valid("example.com:443"));
	EXPECT_FALSE(dns_name_valid("10.0.2.2"));
	EXPECT_FALSE(dns_name_valid("127.1"));
	EXPECT_FALSE(dns_name_valid("0x7f.1"));
	EXPECT_FALSE(dns_name_valid("2130706433"));

	memset(label, 'a', 63);
	label[63] = '\0';
	ASSERT_TRUE(strsprintf(name, sizeof(name), "%s.example", label) > 0);
	EXPECT_TRUE(dns_name_valid(name));
	memset(label, 'a', 64);
	label[64] = '\0';
	ASSERT_TRUE(strsprintf(name, sizeof(name), "%s.example", label) > 0);
	EXPECT_FALSE(dns_name_valid(name));

	memset(label, 'a', 63);
	label[63] = '\0';
	ASSERT_TRUE(strsprintf(name, sizeof(name), "%s.%s.%s.%.61s", label,
			       label, label, label) == 253);
	EXPECT_TRUE(dns_name_valid(name));
	ASSERT_TRUE(strsprintf(name, sizeof(name), "%s.%s.%s.%.62s", label,
			       label, label, label) == 254);
	EXPECT_FALSE(dns_name_valid(name));
}

static const struct unit_case cases[] = {
	UNIT_CASE(digits_counts_leading_digits),
	UNIT_CASE(decimal_fills_its_span),
	UNIT_CASE(nothing_past_the_span_is_read),
	UNIT_CASE(sign_blank_or_prefix_is_no_number),
	UNIT_CASE(number_past_its_bound_is_out_of_range),
	UNIT_CASE(malformed_number_however_long),
	UNIT_CASE(hex_takes_either_case_and_no_prefix),
	UNIT_CASE(signed_number_spans_the_whole_range),
	UNIT_CASE(signed_number_keeps_bounds_and_form),
	UNIT_CASE(strscpy_copies_what_fits_whole),
	UNIT_CASE(strscpy_cuts_and_says_so),
	UNIT_CASE(strsprintf_writes_what_fits_whole),
	UNIT_CASE(strsprintf_leaves_nothing_when_cut),
	UNIT_CASE(xasprintf_allocates_the_size_needed),
	UNIT_CASE(dns_name_is_ldh_labels_and_no_address),
	{}
};

UNIT_SUITE(string, cases);
