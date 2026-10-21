// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * A .bee as bpm install unpacks it and as the operator reads its
 * build-environment.json and certificate.
 */

#include <ftw.h>
#include <stdlib.h>

#include "bpm_package.h"
#include "fuzz.h"
#include "malloc.h"

static char base[64];
static char dest[sizeof(base) + 4];

static int remove_one(const char *path, const struct stat *st, int flag,
		      struct FTW *ftw)
{
	if (ftw->level)
		remove(path);
	return 0;
}

static void empty_base(void)
{
	nftw(base, remove_one, 16, FTW_DEPTH | FTW_PHYS);
}

static void remove_base(void)
{
	empty_base();
	rmdir(base);
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	snprintf(base, sizeof(base), "/tmp/shirudo-fuzz-bpm-package-XXXXXX");
	if (!mkdtemp(base))
		abort();
	snprintf(dest, sizeof(dest), "%s/pkg", base);
	atexit(remove_base);
	return 0;
}

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct bpm_package_info info = {};
	char path[64];
	int fd;

	fd = fuzz_gzip_file(data, size, path, sizeof(path));
	if (fd < 0)
		return 0;
	if (!bpm_pkg_load_info(path, &info))
		xfree(info.cert, info.cert_len + 1);
	bpm_pkg_extract(path, dest);
	empty_base();
	close(fd);
	return 0;
}
