// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <linux/bpf.h>

#include "identity.h"
#include "unit.h"

static void kernel_name_is_the_start_of_a_long_one(void)
{
	char name[BPF_OBJ_NAME_LEN] = "shirudo_bpffs_s";

	EXPECT_TRUE(guard_name_matches(name, "shirudo_bpffs_sb"));
	EXPECT_TRUE(guard_name_matches(name, "shirudo_bpffs_s"));
	EXPECT_TRUE(guard_name_matches(name, "shirudo_bpffs_s_and_more"));
	EXPECT_FALSE(guard_name_matches(name, "shirudo_bpffs_"));
	EXPECT_FALSE(guard_name_matches(name, "shirudo_bpffs_x"));
	EXPECT_FALSE(guard_name_matches(name, ""));
}

static void short_name_is_the_whole_one(void)
{
	char name[BPF_OBJ_NAME_LEN] = "shirudo_tasks";

	EXPECT_TRUE(guard_name_matches(name, "shirudo_tasks"));
	EXPECT_FALSE(guard_name_matches(name, "shirudo_task"));
	EXPECT_FALSE(guard_name_matches(name, "shirudo_tasks_"));
	EXPECT_FALSE(guard_name_matches(name, "shirudo_task_policy"));
	EXPECT_FALSE(guard_name_matches(name, "SHIRUDO_TASKS"));
}

static void unterminated_name_matches_nothing(void)
{
	char name[BPF_OBJ_NAME_LEN];

	memset(name, 'x', sizeof(name));
	EXPECT_FALSE(guard_name_matches(name, "xxxxxxxxxxxxxxx"));
	EXPECT_FALSE(guard_name_matches(name, "xxxxxxxxxxxxxxxx"));
	memset(name, 0, sizeof(name));
	EXPECT_TRUE(guard_name_matches(name, ""));
	EXPECT_FALSE(guard_name_matches(name, "x"));
}

static const struct unit_case cases[] = {
	UNIT_CASE(kernel_name_is_the_start_of_a_long_one),
	UNIT_CASE(short_name_is_the_whole_one),
	UNIT_CASE(unterminated_name_matches_nothing),
	{}
};

UNIT_SUITE(identity, cases);
