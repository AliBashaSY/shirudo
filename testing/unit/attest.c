// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>

#include "attest.h"
#include "unit.h"

static void pcrs_are_a_list_up_to_23(void)
{
	__u32 mask;

	ASSERT_EQ(attest_parse_pcrs("0", &mask), 0);
	EXPECT_EQ(mask, 1U);
	ASSERT_EQ(attest_parse_pcrs("0,9,23", &mask), 0);
	EXPECT_EQ(mask, 1U | 1U << 9 | 1U << 23);
	ASSERT_EQ(attest_parse_pcrs("9,9", &mask), 0);
	EXPECT_EQ(mask, 1U << 9);
	EXPECT_EQ(attest_parse_pcrs("", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("24", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("1,,2", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs(",1", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("1,", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("1,2,", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("1 ", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("+1", &mask), -EINVAL);
	EXPECT_EQ(attest_parse_pcrs("0x1", &mask), -EINVAL);
}

static void pcr_value_is_a_pcr_and_an_equals(void)
{
	const char *value;
	__u64 pcr;

	ASSERT_EQ(attest_parse_pcr_value("15=ab", &pcr, &value), 0);
	EXPECT_EQ(pcr, 15);
	EXPECT_STREQ(value, "ab");
	ASSERT_EQ(attest_parse_pcr_value("0=", &pcr, &value), 0);
	EXPECT_STREQ(value, "");
	EXPECT_EQ(attest_parse_pcr_value("24=ab", &pcr, &value), -EINVAL);
	EXPECT_EQ(attest_parse_pcr_value("=ab", &pcr, &value), -EINVAL);
	EXPECT_EQ(attest_parse_pcr_value("15", &pcr, &value), -EINVAL);
	EXPECT_EQ(attest_parse_pcr_value("1 5=ab", &pcr, &value), -EINVAL);
}

static const struct unit_case cases[] = {
	UNIT_CASE(pcrs_are_a_list_up_to_23),
	UNIT_CASE(pcr_value_is_a_pcr_and_an_equals),
	{}
};

UNIT_SUITE(attest, cases);
