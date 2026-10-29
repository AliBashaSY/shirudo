// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>

#include "hex.h"
#include "unit.h"

static void encode_is_lower_case(void)
{
	static const __u8 in[] = { 0x00, 0x0a, 0xde, 0xad, 0xbe, 0xff };
	char out[2 * sizeof(in) + 1];

	hex_encode(in, sizeof(in), out);
	EXPECT_STREQ(out, "000adeadbeff");
}

static void decode_takes_either_case(void)
{
	static const __u8 want[] = { 0xde, 0xad, 0xbe };
	__u8 out[3];

	ASSERT_EQ(hex_decode("0aFf", out, 2), 0);
	EXPECT_EQ(out[0], 0x0a);
	EXPECT_EQ(out[1], 0xff);
	ASSERT_EQ(hex_decode("DEadBE", out, 3), 0);
	EXPECT_MEMEQ(out, want, sizeof(want));
}

static void decode_wants_the_exact_length(void)
{
	__u8 out[2];

	EXPECT_EQ(hex_decode("abc", out, 1), -EINVAL);
	EXPECT_EQ(hex_decode("abcd", out, 1), -EINVAL);
}

static void decode_refuses_non_hex(void)
{
	__u8 out[1];

	EXPECT_EQ(hex_decode("zz", out, 1), -EINVAL);
}

static const struct unit_case cases[] = {
	UNIT_CASE(encode_is_lower_case),
	UNIT_CASE(decode_takes_either_case),
	UNIT_CASE(decode_wants_the_exact_length),
	UNIT_CASE(decode_refuses_non_hex),
	{}
};

UNIT_SUITE(hex, cases);
