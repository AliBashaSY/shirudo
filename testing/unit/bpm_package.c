// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>

#include <shirudo/string.h>

#include "bpm_oci.h"
#include "bpm_package.h"
#include "malloc.h"
#include "unit.h"

#define DIGEST "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define ENTRY(name, size)						\
	"{\"name\":\"" name "\",\"version\":\"1.0.0\","			\
	"\"repository\":\"shirudo/pkg\","				\
	"\"manifest_digest\":\"" DIGEST "\","				\
	"\"package_digest\":\"" DIGEST "\"" size "}"

static int index_of(const char *text, int *n)
{
	struct bpm_index_entry *entries;
	int ret;

	entries = xcalloc(BPM_MAX_ENTRIES, sizeof(*entries));
	if (!entries)
		return -ENOMEM;
	*n = -1;
	ret = bpm_index_parse((const __u8 *)text, strlen(text), entries, n);
	xfree(entries, BPM_MAX_ENTRIES * sizeof(*entries));
	return ret;
}

/*
 * Every entry carries the size of its package, which the target holds
 * the download to: the first one may not leave it out any more than the
 * others.
 */
static void index_entry_needs_its_package_size(void)
{
	int n;

	ASSERT_EQ(index_of("{\"schema\":1,\"packages\":["
			   ENTRY("a", ",\"package_size\":10") ","
			   ENTRY("b", ",\"package_size\":20") "]}", &n), 0);
	EXPECT_EQ(n, 2);
	EXPECT_EQ(index_of("{\"schema\":1,\"packages\":["
			   ENTRY("a", "") "]}", &n), -EINVAL);
	EXPECT_EQ(index_of("{\"schema\":1,\"packages\":["
			   ENTRY("a", ",\"package_size\":10") ","
			   ENTRY("b", "") "]}", &n), -EINVAL);
	EXPECT_EQ(index_of("{\"schema\":1,\"packages\":["
			   ENTRY("a", ",\"package_size\":null") "]}", &n),
		  -EINVAL);
	EXPECT_EQ(index_of("{\"schema\":1,\"packages\":["
			   ENTRY("a", ",\"package_size\":0") "]}", &n),
		  -EINVAL);
}

static const struct unit_case cases[] = {
	UNIT_CASE(index_entry_needs_its_package_size),
	{}
};

UNIT_SUITE(bpm_package, cases);
