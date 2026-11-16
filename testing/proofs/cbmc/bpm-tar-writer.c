// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * tar_entry() of tools/shirudo-build/bpm-build.c on every name of LEN
 * bytes, size and mode: a member is refused before a byte is written, or
 * its header is one block the readers of util/bpm_package.c take, with the
 * same name, size and type, and the next header lies where they look.
 */

/* As proof.h, before the system headers can include <ctype.h>. */
#define __NO_CTYPE 1

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#define PROOF_MEMCPY_BYTEWISE
#include "proof.h"
#include "../../../util/string.c"
#include "../../../util/bpm_package.c"
#include "../../../tools/shirudo-build/bpm-build.c"

#define TAR_BLOCK	512
#define TAR_NAME	100
#define TAR_SIZE_MAX	077777777777ULL
/* The last bytes of a name that are free where the checksum is proven. */
#define FREE		16

/* The member tar_entry() is given, and what reached the archive. */
static const char *member;
static size_t member_len;
static mode_t member_mode;
static char member_type;
static __u64 archived;
static bool headed, summing;

/*
 * A field as the reader's bpm_tar_octal() takes it, from a copy: its
 * search for the digits would go through the whole header otherwise.
 */
static bool octal_field(const __u8 *h, size_t off, size_t n, __u64 want)
{
	char field[12];
	size_t i;
	__u64 v;

	for (i = 0; i < n; i++)
		field[i] = (char)h[off + i];
	return !bpm_tar_octal(field, n, &v) && v == want;
}

/* A header as bpm_pkg_extract() and the member readers take it. */
static void header_reads_back(const __u8 *h)
{
	if (summing) {
		__CPROVER_assert(bpm_tar_header_ok(h),
				 "the checksum the reader computes");
		return;
	}
	__CPROVER_assert(memchr(h, '\0', TAR_NAME) && !h[345],
			 "a name terminated in its field, no prefix");
	__CPROVER_assert(!strcmp((const char *)h, member),
			 "the name read back");
	__CPROVER_assert(octal_field(h, 124, 12, member_len),
			 "the size read back");
	__CPROVER_assert(h[156] == member_type, "the type read back");
	__CPROVER_assert(octal_field(h, 100, 8, member_mode & 07777) &&
			 octal_field(h, 108, 8, 0) &&
			 octal_field(h, 116, 8, 0) && octal_field(h, 136, 12, 0),
			 "the permission bits, uid, gid and mtime 0");
	__CPROVER_assert(!memcmp(h + 257, "ustar", 6) &&
			 !memcmp(h + 263, "00", 2), "a ustar header");
}

/*
 * zlib's gzwrite(), every write taken whole: the first block is the
 * header, checked while tar_entry() still holds it, the rest is counted.
 */
int gzwrite(gzFile file, voidpc buf, __u32 len)
{
	if (!headed) {
		__CPROVER_assert(len == TAR_BLOCK, "a header of one block first");
		header_reads_back(buf);
		headed = true;
	}
	archived += len;
	return (int)len;
}

static void member_written(const char *name, bool checksum)
{
	const size_t name_len = LEN;
	struct bpm_tar tar;
	mode_t mode;
	size_t len;
	__u64 pad;
	char type;
	int ret;

	/* build_archive() writes files, and one directory without content. */
	__CPROVER_assume(type == '0' || (type == '5' && !len));
	member = name;
	member_len = len;
	member_mode = mode;
	member_type = type;
	summing = checksum;
	headed = false;
	archived = 0;
	ret = tar_entry(&tar, name, NULL, len, mode, type);
	if (checksum)
		return;
	if (name_len >= TAR_NAME || len > TAR_SIZE_MAX) {
		__CPROVER_assert(ret && !headed,
				 "refused before a byte is written");
		return;
	}
	__CPROVER_assert(headed, "the header written");
	/* One gzwrite() takes INT_MAX bytes; build_read_file() reads 256 MiB. */
	__CPROVER_assert(!ret == (len <= INT_MAX),
			 "written whole, up to what one write takes");
	if (ret)
		return;
	pad = (TAR_BLOCK - len % TAR_BLOCK) % TAR_BLOCK;
	__CPROVER_assert(archived == TAR_BLOCK + len + pad,
			 "the next header where the readers look for it");
}

/* tar_octal() alone: tar_entry() gives it no value wider than its field. */
static void octal_written(size_t size)
{
	__u8 field[12];
	__u64 value;
	int ret;

	ret = tar_octal(field, 0, size, value);
	__CPROVER_assert(!ret == !(value >> (3 * (size - 1))),
			 "refused exactly with more digits than the field");
	__CPROVER_assert(ret || octal_field(field, 0, size, value),
			 "a value that fits read back");
}

void harness(void)
{
	char *name;
	size_t i;

	octal_written(8);
	octal_written(12);
	name = any_bytes(LEN + 1);
	name[LEN] = '\0';
	__CPROVER_assume(strnlen(name, LEN) == LEN);
	member_written(name, false);
	if (LEN >= TAR_NAME)
		return;
	/*
	 * The checksum apart, for names that end in FREE bytes of any value:
	 * the solver holds the writer's sum against the reader's from the
	 * first free byte on, and through a whole name that takes minutes.
	 */
	for (i = 0; i + FREE < LEN; i++)
		name[i] = 'a';
	member_written(name, true);
}
