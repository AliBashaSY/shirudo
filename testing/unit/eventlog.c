// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <shirudo/string.h>

#include "eventlog.h"
#include "io.h"
#include "json.h"
#include "unit.h"

static char dir[4096], log_path[4200], pending_path[4200];

static void use_log(void)
{
	const char *tmp = getenv("TMPDIR");

	ASSERT_TRUE(strsprintf(dir, sizeof(dir),
			       "%s/shirudo-unit-eventlog-XXXXXX",
			       tmp && tmp[0] ? tmp : "/tmp") > 0);
	ASSERT_TRUE(mkdtemp(dir));
	ASSERT_TRUE(strsprintf(log_path, sizeof(log_path), "%s/eventlog",
			       dir) > 0);
	ASSERT_TRUE(strsprintf(pending_path, sizeof(pending_path),
			       "%s/pending", dir) > 0);
	ASSERT_EQ(setenv("SHIRUDO_EVENTLOG_PATH", log_path, 1), 0);
	ASSERT_EQ(setenv("SHIRUDO_EVENTLOG_PENDING_PATH", pending_path, 1), 0);
}

static void drop_log(void)
{
	unlink(log_path);
	unlink(pending_path);
	rmdir(dir);
}

static void record(char *buf, size_t cap, const char *type, __u8 fill)
{
	struct json_emit e = { .buf = buf, .cap = cap };
	__u8 digest[32];

	memset(digest, fill, sizeof(digest));
	eventlog_record_open(&e, type, digest);
	json_obj_close(&e);
	ASSERT_EQ(e.err, 0);
}

static void write_log(const char *text, size_t len)
{
	int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

	ASSERT_TRUE(fd >= 0);
	ASSERT_EQ(write_complete(fd, text, len), 0);
	ASSERT_EQ(close(fd), 0);
}

static void replay_of(const char *a, const char *b, __u8 out[32])
{
	char text[2048];
	int len;

	len = strsprintf(text, sizeof(text), "%s\n%s%s", a, b, b[0] ? "\n" : "");
	ASSERT_TRUE(len > 0);
	write_log(text, (size_t)len);
	ASSERT_EQ(eventlog_replay(out), 0);
}

/*
 * A record killed before its newline is no record yet: readers skip it,
 * and the next append drops it rather than run on from it.
 */
static void unfinished_append_is_no_record(void)
{
	char init[1024], load[1024], unload[1024], text[2048];
	__u8 alone[32], both[32], got[32];
	int len;

	use_log();
	record(init, sizeof(init), "init", 1);
	record(load, sizeof(load), "upatch_load", 2);
	record(unload, sizeof(unload), "upatch_unload", 3);
	replay_of(init, "", alone);
	replay_of(init, unload, both);

	len = strsprintf(text, sizeof(text), "%s\n%.*s", init,
			 (int)strlen(load) - 1, load);
	ASSERT_TRUE(len > 0);
	write_log(text, (size_t)len);
	ASSERT_EQ(eventlog_replay(got), 0);
	EXPECT_MEMEQ(got, alone, 32);

	len = strsprintf(text, sizeof(text), "%s\n%s", init, load);
	ASSERT_TRUE(len > 0);
	write_log(text, (size_t)len);
	ASSERT_EQ(eventlog_replay(got), 0);
	EXPECT_MEMEQ(got, alone, 32);
	ASSERT_EQ(eventlog_append_to(log_path, unload), 0);
	ASSERT_EQ(eventlog_replay(got), 0);
	EXPECT_MEMEQ(got, both, 32);
	drop_log();
}

/* A NUL would end the walk early and hide the records after it. */
static void nul_in_the_log_is_refused(void)
{
	char init[1024], unload[1024], text[2048];
	__u8 got[32];
	int len;

	use_log();
	record(init, sizeof(init), "init", 1);
	record(unload, sizeof(unload), "upatch_unload", 3);
	len = strsprintf(text, sizeof(text), "%s\n_%s\n", init, unload);
	ASSERT_TRUE(len > 0);
	text[strlen(init) + 1] = '\0';
	write_log(text, (size_t)len);
	EXPECT_EQ(eventlog_replay(got), -EINVAL);
	drop_log();
}

static const struct unit_case cases[] = {
	UNIT_CASE(unfinished_append_is_no_record),
	UNIT_CASE(nul_in_the_log_is_refused),
	{}
};

UNIT_SUITE(eventlog, cases);
