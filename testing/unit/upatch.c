// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>

#include "unit.h"
#include "upatch.h"

#define ONE_PROG_LEN \
	(sizeof(struct upatch_prog_metadata) + sizeof(struct upatch_prog_entry))

static void one_raw_tracepoint(__u8 *buf, __u32 nr_maps)
{
	struct upatch_prog_metadata md = {};
	struct upatch_prog_entry prog = {};

	md.magic = cpu_to_le32(UPATCH_PROG_MAGIC);
	md.version = cpu_to_le32(UPATCH_PROG_VERSION);
	md.nr_progs = cpu_to_le32(1);
	md.nr_maps = cpu_to_le32(nr_maps);
	prog.role = cpu_to_le32(UPATCH_PROG_ROLE_ATTACH);
	prog.kind = cpu_to_le32(UPATCH_ATTACH_RAW_TRACEPOINT);
	memcpy(buf, &md, sizeof(md));
	memcpy(buf + sizeof(md), &prog, sizeof(prog));
}

static void prog_specs_take_the_signed_counts(void)
{
	struct prog_spec *specs = NULL;
	__u8 buf[ONE_PROG_LEN];

	one_raw_tracepoint(buf, 3);
	ASSERT_EQ(prog_specs_parse(buf, sizeof(buf), 3, 1, &specs), 0);
	ASSERT_TRUE(specs);
	EXPECT_UEQ(specs[0].role, UPATCH_PROG_ROLE_ATTACH);
	EXPECT_UEQ(specs[0].kind, UPATCH_ATTACH_RAW_TRACEPOINT);
	EXPECT_EQ(specs[0].target_fd, -1);
	prog_specs_free(specs, 1);
}

static void prog_specs_refuse_another_header_count(void)
{
	struct prog_spec *specs = NULL;
	__u8 buf[ONE_PROG_LEN];

	one_raw_tracepoint(buf, 3);
	EXPECT_EQ(prog_specs_parse(buf, sizeof(buf), 4, 1, &specs), -EINVAL);
	EXPECT_FALSE(specs);
	EXPECT_EQ(prog_specs_parse(buf, sizeof(buf), 2, 1, &specs), -EINVAL);
	EXPECT_EQ(prog_specs_parse(buf, sizeof(buf), 3, 2, &specs), -EINVAL);
	EXPECT_EQ(prog_specs_parse(buf, sizeof(buf), 3, 0, &specs), -EINVAL);
	EXPECT_FALSE(specs);
}

static const struct unit_case cases[] = {
	UNIT_CASE(prog_specs_take_the_signed_counts),
	UNIT_CASE(prog_specs_refuse_another_header_count),
	{}
};

UNIT_SUITE(upatch, cases);
