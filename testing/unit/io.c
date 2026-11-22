// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <shirudo/string.h>

#include "../../util/paths.h"

static char etc_dir[4096];

#undef ETC_DIR
#define ETC_DIR etc_dir

#include "../../util/io.c"

#include "unit.h"

static int log_with(const char *text, size_t len)
{
	const char *dir = getenv("TMPDIR");
	char path[4096];
	int fd;

	ASSERT_TRUE(strsprintf(path, sizeof(path), "%s/shirudo-unit-io-XXXXXX",
			       dir && dir[0] ? dir : "/tmp") > 0);
	fd = mkstemp(path);
	ASSERT_TRUE(fd >= 0);
	ASSERT_EQ(unlink(path), 0);
	ASSERT_EQ(fcntl(fd, F_SETFL, O_APPEND), 0);
	ASSERT_EQ(write_complete(fd, text, len), 0);
	return fd;
}

static void expect_log(int fd, const char *want)
{
	char buf[16384];
	ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);

	ASSERT_TRUE(n >= 0);
	buf[n] = '\0';
	EXPECT_STREQ(buf, want);
}

static void lines_len_ends_at_the_last_newline(void)
{
	EXPECT_EQ(lines_len("", 0), 0);
	EXPECT_EQ(lines_len("abc", 3), 0);
	EXPECT_EQ(lines_len("a\n", 2), 2);
	EXPECT_EQ(lines_len("a\nb", 3), 2);
	EXPECT_EQ(lines_len("a\nb\n", 4), 4);
	EXPECT_EQ(lines_len("\n\n", 2), 2);
}

static void append_line_adds_one_record(void)
{
	int fd = log_with("", 0);

	EXPECT_EQ(append_line(fd, "log", "x", 1), 0);
	EXPECT_EQ(append_line(fd, "log", "y\n", 2), 0);
	expect_log(fd, "x\ny\n");
	close(fd);
}

/*
 * A record killed before its newline would run into the next one on
 * its line, and every reader after would refuse the log.
 */
static void append_line_drops_what_did_not_finish(void)
{
	char text[12000];
	int fd;

	fd = log_with("x\n{\"half\":", 10);
	EXPECT_EQ(append_line(fd, "log", "z", 1), 0);
	expect_log(fd, "x\nz\n");
	close(fd);

	fd = log_with("{\"whole\":1}", 11);
	EXPECT_EQ(append_line(fd, "log", "z", 1), 0);
	expect_log(fd, "z\n");
	close(fd);

	memcpy(text, "x\n", 2);
	memset(text + 2, 'a', sizeof(text) - 2);
	fd = log_with(text, sizeof(text));
	EXPECT_EQ(append_line(fd, "log", "z", 1), 0);
	expect_log(fd, "x\nz\n");
	close(fd);
}

static void append_line_takes_one_line_alone(void)
{
	int fd = log_with("x\n", 2);

	EXPECT_EQ(append_line(fd, "log", "a\nb", 3), -EINVAL);
	EXPECT_EQ(append_line(fd, "log", "a\0b", 3), -EINVAL);
	expect_log(fd, "x\n");
	close(fd);
}

static void lock_exclusive_keeps_others_out(void)
{
	int fd = log_with("", 0), other;
	char path[64];

	ASSERT_TRUE(strsprintf(path, sizeof(path), "/proc/self/fd/%d", fd) > 0);
	other = open(path, O_RDONLY | O_CLOEXEC);
	ASSERT_TRUE(other >= 0);
	EXPECT_EQ(lock_exclusive(fd), 0);
	EXPECT_EQ(flock(other, LOCK_EX | LOCK_NB), -1);
	EXPECT_EQ(errno, EWOULDBLOCK);
	close(fd);
	EXPECT_EQ(lock_exclusive(other), 0);
	close(other);
}

static void kernel_dev_is_major_over_20_minor_bits(void)
{
	EXPECT_EQ(kernel_dev(makedev(8, 1)), 0x800001U);
	EXPECT_EQ(kernel_dev(makedev(253, 0x12345)), 253U << 20 | 0x12345U);
	EXPECT_EQ(kernel_dev(makedev(0xfff, 0xfffff)), 0xffffffffU);
}

static void bpffs_mounted_at_wants_the_root_of_a_bpffs(void)
{
	const char *dir = getenv("TMPDIR");

	EXPECT_EQ(bpffs_mounted_at(dir && dir[0] ? dir : "/tmp"), 0);
	EXPECT_EQ(bpffs_mounted_at("/"), 0);
	EXPECT_EQ(bpffs_mounted_at("/nonexistent/shirudo"), 0);
	EXPECT_EQ(bpffs_mounted_at("/dev/null"), -ENOTDIR);
}

static int etc_fd = -1;

static int remove_entry(const char *path, const struct stat *st, int type,
			struct FTW *ftw)
{
	return remove(path);
}

static void remove_etc(void)
{
	close(etc_fd);
	ASSERT_EQ(nftw(etc_dir, remove_entry, 16, FTW_DEPTH | FTW_PHYS), 0);
}

static void make_etc_in(const char *dir)
{
	ASSERT_TRUE(strsprintf(etc_dir, sizeof(etc_dir),
			       "%s/shirudo-unit-etc-XXXXXX", dir) > 0);
	ASSERT_TRUE(mkdtemp(etc_dir));
	etc_fd = open(etc_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	ASSERT_TRUE(etc_fd >= 0);
}

static void make_dir(const char *name, mode_t mode)
{
	ASSERT_EQ(mkdirat(etc_fd, name, 0700), 0);
	ASSERT_EQ(fchmodat(etc_fd, name, mode, 0), 0);
}

static void make_file(const char *name, mode_t mode)
{
	int fd = openat(etc_fd, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
			0600);

	ASSERT_TRUE(fd >= 0);
	ASSERT_EQ(fchmod(fd, mode), 0);
	close(fd);
}

static void make_link(const char *target, const char *name)
{
	ASSERT_EQ(symlinkat(target, etc_fd, name), 0);
}

/* check_trusted() of the file opened as one name and asked about as another. */
static int trusted(const char *opened, const char *asked, bool secret)
{
	int fd = openat(etc_fd, opened, O_RDONLY | O_CLOEXEC), ret;
	char path[PATH_MAX];

	ASSERT_TRUE(fd >= 0);
	ASSERT_TRUE(strsprintf(path, sizeof(path), "%s/%s", etc_dir, asked) > 0);
	ret = check_trusted(fd, path, secret);
	close(fd);
	return ret;
}

/* The check walks up to /, so the tree goes where every directory passes it. */
static void make_etc(void)
{
	const char *dir = getenv("TMPDIR");

	make_etc_in(dir && dir[0] ? dir : "/tmp");
	make_file("probe", 0644);
	if (!trusted("probe", "probe", false))
		return;
	remove_etc();
	make_etc_in("/tmp");
}

/* As config managers write: the new file beside its name, renamed over it. */
static void replace_mode(const char *name, const char *text, mode_t mode)
{
	char tmp[PATH_MAX];
	int fd;

	ASSERT_TRUE(strsprintf(tmp, sizeof(tmp), "%s.tmp", name) > 0);
	fd = openat(etc_fd, tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		    0600);
	ASSERT_TRUE(fd >= 0);
	ASSERT_EQ(fchmod(fd, mode), 0);
	ASSERT_EQ(write_complete(fd, text, strlen(text)), 0);
	close(fd);
	ASSERT_EQ(renameat(etc_fd, tmp, etc_fd, name), 0);
}

static void check_trusted_takes_a_file_nobody_else_can_change(void)
{
	make_etc();
	make_dir("keys", 0755);
	make_file("keys/cert.der", 0644);
	EXPECT_EQ(trusted("keys/cert.der", "keys/cert.der", false), 0);
	EXPECT_EQ(trusted("keys/cert.der", "keys//./cert.der", false), 0);
	EXPECT_EQ(trusted("keys/cert.der", "keys/../keys/cert.der", false), 0);
	EXPECT_EQ(trusted("keys/cert.der", "keys/cert.der", true), -EPERM);
	ASSERT_EQ(fchmodat(etc_fd, "keys/cert.der", 0600, 0), 0);
	EXPECT_EQ(trusted("keys/cert.der", "keys/cert.der", true), 0);
	ASSERT_EQ(fchmodat(etc_fd, "keys/cert.der", 0664, 0), 0);
	EXPECT_EQ(trusted("keys/cert.der", "keys/cert.der", false), -EPERM);
	remove_etc();
}

static void check_trusted_refuses_a_directory_others_can_write(void)
{
	make_etc();
	make_dir("keys", 0775);
	make_file("keys/cert.der", 0644);
	EXPECT_EQ(trusted("keys/cert.der", "keys/cert.der", false), -EPERM);
	/* A sticky directory guards its entries only when it is root's. */
	ASSERT_EQ(fchmodat(etc_fd, "keys", 01777, 0), 0);
	EXPECT_EQ(trusted("keys/cert.der", "keys/cert.der", false),
		  geteuid() ? -EPERM : 0);
	remove_etc();
}

static void check_trusted_follows_a_link_nobody_else_made(void)
{
	char target[PATH_MAX];

	make_etc();
	make_dir("keys", 0700);
	make_file("keys/0a.der", 0644);
	ASSERT_TRUE(strsprintf(target, sizeof(target), "%s/keys/0a.der",
			       etc_dir) > 0);
	make_link(target, "keys/cert.der");
	EXPECT_EQ(trusted("keys/0a.der", "keys/cert.der", false), 0);
	make_link("../keys/0a.der", "keys/up.der");
	EXPECT_EQ(trusted("keys/0a.der", "keys/up.der", false), 0);
	make_link("keys", "current");
	EXPECT_EQ(trusted("keys/0a.der", "current/cert.der", false), 0);
	make_link("loop.der", "keys/loop.der");
	EXPECT_EQ(trusted("keys/0a.der", "keys/loop.der", false), -ELOOP);
	make_link("0a.der/more", "keys/past.der");
	EXPECT_EQ(trusted("keys/0a.der", "keys/past.der", false), -ENOTDIR);
	remove_etc();
}

static void check_trusted_refuses_a_link_others_can_replace(void)
{
	make_etc();
	make_dir("keys", 0700);
	make_file("keys/0a.der", 0644);
	make_dir("open", 0777);
	make_link("../keys/0a.der", "open/cert.der");
	EXPECT_EQ(trusted("keys/0a.der", "open/cert.der", false), -EPERM);
	make_file("open/0b.der", 0644);
	make_link("../open/0b.der", "keys/cert.der");
	EXPECT_EQ(trusted("open/0b.der", "keys/cert.der", false), -EPERM);
	/* In a sticky directory of root's a link is its owner's to replace. */
	if (!geteuid()) {
		ASSERT_EQ(fchmodat(etc_fd, "open", 01777, 0), 0);
		EXPECT_EQ(trusted("keys/0a.der", "open/cert.der", false), 0);
		ASSERT_EQ(fchownat(etc_fd, "open/cert.der", 65534, 65534,
				   AT_SYMLINK_NOFOLLOW), 0);
		EXPECT_EQ(trusted("keys/0a.der", "open/cert.der", false),
			  -EPERM);
	}
	remove_etc();
}

/* What a name swapped after the open leaves: one file open, another named. */
static void check_trusted_wants_the_file_the_path_names(void)
{
	make_etc();
	make_dir("keys", 0700);
	make_file("keys/cert.der", 0644);
	make_dir("open", 0777);
	make_file("open/cert.der", 0644);
	EXPECT_EQ(trusted("open/cert.der", "keys/cert.der", false), -ESTALE);
	EXPECT_EQ(trusted("keys/cert.der", "keys", false), -EISDIR);
	remove_etc();
}

/*
 * open_checked() with an open that renames a new file over the one it
 * opened, for its first race.renames opens: the race between the open
 * and the check, on cue.
 */
static struct race {
	const char *name;
	mode_t mode;
	int renames;
	int opens;
} race;

static int open_then_replace(const char *path, int flags)
{
	int fd = open_path(path, flags);

	if (fd >= 0 && race.opens++ < race.renames)
		replace_mode(race.name, "b\n", race.mode);
	return fd;
}

static int open_racing(const char *name, mode_t mode, int renames)
{
	char path[PATH_MAX];

	race = (struct race){ .name = name, .mode = mode, .renames = renames };
	ASSERT_TRUE(strsprintf(path, sizeof(path), "%s/%s", etc_dir,
			       name) > 0);
	return open_checked(path, O_RDONLY, false, open_then_replace);
}

static void open_trusted_reopens_a_file_renamed_over(void)
{
	char text[4] = {};
	int fd;

	make_etc();
	make_dir("keys", 0755);
	replace_mode("keys/cert.der", "a\n", 0644);
	fd = open_racing("keys/cert.der", 0644, 1);
	ASSERT_TRUE(fd >= 0);
	EXPECT_EQ(race.opens, 2);
	EXPECT_EQ(pread(fd, text, sizeof(text) - 1, 0), 2);
	EXPECT_STREQ(text, "b\n");
	close(fd);
	fd = open_racing("keys/cert.der", 0644, TRUSTED_REOPENS);
	ASSERT_TRUE(fd >= 0);
	EXPECT_EQ(race.opens, TRUSTED_REOPENS + 1);
	close(fd);
	remove_etc();
}

static void open_trusted_gives_up_on_a_name_that_keeps_changing(void)
{
	make_etc();
	make_dir("keys", 0755);
	replace_mode("keys/cert.der", "a\n", 0644);
	EXPECT_EQ(open_racing("keys/cert.der", 0644,
			      TRUSTED_REOPENS + 1), -ESTALE);
	EXPECT_EQ(race.opens, TRUSTED_REOPENS + 1);
	remove_etc();
}

/* A reopen checks the new file in full, and only a rename is reopened. */
static void open_trusted_checks_each_file_it_opens(void)
{
	make_etc();
	make_dir("keys", 0755);
	replace_mode("keys/cert.der", "a\n", 0644);
	EXPECT_EQ(open_racing("keys/cert.der", 0664, 1), -EPERM);
	EXPECT_EQ(race.opens, 2);
	/* The file the rename left is group-writable. */
	EXPECT_EQ(open_racing("keys/cert.der", 0644, 0), -EPERM);
	EXPECT_EQ(race.opens, 1);
	remove_etc();
}

static const struct unit_case cases[] = {
	UNIT_CASE(lines_len_ends_at_the_last_newline),
	UNIT_CASE(append_line_adds_one_record),
	UNIT_CASE(append_line_drops_what_did_not_finish),
	UNIT_CASE(append_line_takes_one_line_alone),
	UNIT_CASE(lock_exclusive_keeps_others_out),
	UNIT_CASE(kernel_dev_is_major_over_20_minor_bits),
	UNIT_CASE(bpffs_mounted_at_wants_the_root_of_a_bpffs),
	UNIT_CASE(check_trusted_takes_a_file_nobody_else_can_change),
	UNIT_CASE(check_trusted_refuses_a_directory_others_can_write),
	UNIT_CASE(check_trusted_follows_a_link_nobody_else_made),
	UNIT_CASE(check_trusted_refuses_a_link_others_can_replace),
	UNIT_CASE(check_trusted_wants_the_file_the_path_names),
	UNIT_CASE(open_trusted_reopens_a_file_renamed_over),
	UNIT_CASE(open_trusted_gives_up_on_a_name_that_keeps_changing),
	UNIT_CASE(open_trusted_checks_each_file_it_opens),
	{}
};

UNIT_SUITE(io, cases);
