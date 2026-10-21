/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * What the libFuzzer targets for shirudo's parsers share.
 */

#ifndef SHIRUDO_FUZZ_H
#define SHIRUDO_FUZZ_H

#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <shirudo/misc.h>

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const __u8 *data, size_t size);

/*
 * The input as a file for the parsers that read one by name: a memfd,
 * named through /proc/self/fd. -1, or the descriptor to close after.
 */
static inline int fuzz_file(const __u8 *data, size_t size, char *path,
			    size_t cap)
{
	int fd = memfd_create("fuzz", MFD_CLOEXEC);
	size_t off = 0;

	if (fd < 0)
		return -1;
	while (off < size) {
		ssize_t n = write(fd, data + off, size - off);

		if (n <= 0) {
			close(fd);
			return -1;
		}
		off += (size_t)n;
	}
	snprintf(path, cap, "/proc/self/fd/%d", fd);
	return fd;
}

#endif /* SHIRUDO_FUZZ_H */
