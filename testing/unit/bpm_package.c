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

/*
 * One rule for every member: an archive rooted at ./ unpacked, while
 * the member readers, which want <name>-<version>/<member>, found none.
 */
static void archive_path_has_no_dot_component(void)
{
	EXPECT_TRUE(bpm_archive_rel_ok("t-1.0.0/"));
	EXPECT_TRUE(bpm_archive_rel_ok("t-1.0.0/Beefile"));
	EXPECT_TRUE(bpm_archive_rel_ok("t-1.0.0/a/.b"));
	EXPECT_FALSE(bpm_archive_rel_ok(""));
	EXPECT_FALSE(bpm_archive_rel_ok("/t-1.0.0/Beefile"));
	EXPECT_FALSE(bpm_archive_rel_ok("./"));
	EXPECT_FALSE(bpm_archive_rel_ok("./Beefile"));
	EXPECT_FALSE(bpm_archive_rel_ok("t-1.0.0/./Beefile"));
	EXPECT_FALSE(bpm_archive_rel_ok("t-1.0.0/../Beefile"));
	EXPECT_FALSE(bpm_archive_rel_ok("t-1.0.0//Beefile"));
}

static int sign(int v)
{
	return (v > 0) - (v < 0);
}

/* The precedence example of SemVer 2.0.0, 11.4, and build metadata. */
static void semver_follows_its_precedence(void)
{
	static const char *const order[] = {
		"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta",
		"1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1",
		"1.0.0-rc.2+x", "1.0.0-rc.10", "1.0.0", "1.2.3", "1.10.0",
	};
	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(order); i++) {
		for (j = 0; j < ARRAY_SIZE(order); j++)
			EXPECT_EQ(bpm_semver_compare(order[i], order[j]),
				  sign((int)i - (int)j));
	}
	EXPECT_EQ(bpm_semver_compare("1.0.0-rc.1+b2", "1.0.0-rc.1+b1"), 0);
	EXPECT_EQ(bpm_semver_compare("1.0.0+b2", "1.0.0"), 0);
}

/*
 * Versions SemVer does not describe sort below those it does and by
 * their bytes among themselves: the order is total over all of them.
 */
static void version_order_is_total(void)
{
	static const char *const v[] = {
		"1.", "1.2.3-rc.1", "01.10.0", "1.2", "01.2.0", "1.2.0-rc",
		"1.2.3.4", "1.2.3.5", "1.0.0foo", "1.0.0", "1.0.0-rc.01",
		"1.0.0-rc.1", "0.9.0+build.5", "2.0.0-0.3.7", "1_0", "9",
	};
	size_t a, b, c;

	EXPECT_EQ(bpm_semver_compare("1.2.3.5", "1.2.3.4"), 1);
	EXPECT_EQ(bpm_semver_compare("01.0.0", "0.0.1"), -1);
	EXPECT_EQ(bpm_semver_compare("1.0.0-rc.01", "1.0.0-rc.1"), -1);
	for (a = 0; a < ARRAY_SIZE(v); a++) {
		for (b = 0; b < ARRAY_SIZE(v); b++) {
			int ab = bpm_semver_compare(v[a], v[b]);

			EXPECT_EQ(ab, -bpm_semver_compare(v[b], v[a]));
			for (c = 0; c < ARRAY_SIZE(v); c++) {
				if (ab <= 0 &&
				    bpm_semver_compare(v[b], v[c]) <= 0)
					EXPECT_TRUE(bpm_semver_compare(v[a],
								       v[c]) <= 0);
			}
		}
	}
}

static const struct unit_case cases[] = {
	UNIT_CASE(index_entry_needs_its_package_size),
	UNIT_CASE(semver_follows_its_precedence),
	UNIT_CASE(version_order_is_total),
	UNIT_CASE(archive_path_has_no_dot_component),
	{}
};

UNIT_SUITE(bpm_package, cases);
