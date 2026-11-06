// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>

#include "chain.h"
#include "measured.h"
#include "unit.h"

#define PAYLOAD "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"

/* An init event lists each upatch once, by a name an upatch can have. */
static void set_takes_each_upatch_once(void)
{
	struct measured_upatch set[2];
	size_t n = 0;

	EXPECT_EQ(measured_set_add(set, &n, 2, "a", PAYLOAD), 0);
	EXPECT_EQ(measured_set_add(set, &n, 2, "a", PAYLOAD), -EEXIST);
	EXPECT_EQ(measured_set_add(set, &n, 2, "a\n" PAYLOAD " b", PAYLOAD),
		  -EINVAL);
	EXPECT_EQ(measured_set_add(set, &n, 2, "b", "00"), -EINVAL);
	EXPECT_EQ(measured_set_add(set, &n, 2, "b", PAYLOAD), 0);
	EXPECT_EQ(measured_set_add(set, &n, 2, "c", PAYLOAD), -E2BIG);
	ASSERT_EQ(n, 2);
	EXPECT_STREQ(set[0].name, "a");
	EXPECT_STREQ(set[1].payload_hex, PAYLOAD);
}

static const struct unit_case cases[] = {
	UNIT_CASE(set_takes_each_upatch_once),
	{}
};

UNIT_SUITE(measured, cases);
