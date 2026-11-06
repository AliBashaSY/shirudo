// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include "../../util/bpm_tree_state.c"

#include "unit.h"

static char path[4096];

static void write_config(const char *text, bool gz)
{
	const char *tmp = getenv("TMPDIR");
	int fd;

	ASSERT_TRUE(strsprintf(path, sizeof(path), "%s/shirudo-unit-kconfig-XXXXXX",
			       tmp && tmp[0] ? tmp : "/tmp") > 0);
	fd = mkstemp(path);
	ASSERT_TRUE(fd >= 0);
	if (gz) {
		gzFile f = gzdopen(fd, "wb");

		ASSERT_TRUE(f);
		ASSERT_EQ(gzwrite(f, text, (__u32)strlen(text)),
			  (int)strlen(text));
		ASSERT_EQ(gzclose(f), Z_OK);
	} else {
		ASSERT_EQ(write_complete(fd, text, strlen(text)), 0);
		ASSERT_EQ(close(fd), 0);
	}
}

static const char *modules_in(const char *text, bool gz)
{
	static char values[TREE_MAX_KCONFIG][TREE_STRING_MAX];
	static int slots[TREE_SELECTOR_HASH_SIZE];
	static struct tree_selector sel;
	bool found[TREE_MAX_KCONFIG] = {};
	int ret;

	memset(&sel, 0, sizeof(sel));
	memset(values, 0, sizeof(values));
	strscpy(sel.kconfig[0].key, "CONFIG_MODULES", sizeof(sel.kconfig[0].key));
	sel.n_kconfig = 1;
	selector_hash_build(&sel, slots);
	write_config(text, gz);
	ret = read_selector_config_file(path, &sel, slots, values, found, gz);
	unlink(path);
	ASSERT_EQ(ret, 0);
	return found[0] ? values[0] : "-";
}

/*
 * A line longer than the read buffer came in parts, and the part from
 * the middle of a long string read as an option of its own.
 */
static void long_line_is_one_line(void)
{
	static const char head[] =
		"# CONFIG_MODULES is not set\nCONFIG_CMDLINE=\"";
	static const char tail[] = "CONFIG_MODULES=y\"\nCONFIG_X=y\n";
	char text[4096];

	memcpy(text, head, sizeof(head) - 1);
	memset(text + sizeof(head) - 1, ' ', 2014);
	memcpy(text + sizeof(head) - 1 + 2014, tail, sizeof(tail));
	EXPECT_STREQ(modules_in(text, false), "n");
	EXPECT_STREQ(modules_in(text, true), "n");
	EXPECT_STREQ(modules_in("CONFIG_MODULES=y\n", false), "y");
	EXPECT_STREQ(modules_in("CONFIG_MODULES=m", true), "m");
}

static const struct unit_case cases[] = {
	UNIT_CASE(long_line_is_one_line),
	{}
};

UNIT_SUITE(bpm_tree_state, cases);
