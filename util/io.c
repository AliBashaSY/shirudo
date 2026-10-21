// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include <shirudo/misc.h>

#include "io.h"
#include "malloc.h"

int shirudo_write_all(int fd, const void *buf, size_t len)
{
	const __u8 *p = buf;

	while (len) {
		ssize_t n = write(fd, p, len);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (n == 0)
			return -EIO;
		p += (size_t)n;
		len -= (size_t)n;
	}
	return 0;
}

int shirudo_read_file(const char *path, size_t max, void *out, size_t *len)
{
	struct stat st;
	char *buf, *p;
	size_t left;
	int fd, rc;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	if (fstat(fd, &st) != 0) {
		rc = -errno;
		goto out;
	}
	if (!S_ISREG(st.st_mode)) {
		rc = -EINVAL;
		goto out;
	}
	if ((uintmax_t)st.st_size > max) {
		rc = -EFBIG;
		goto out;
	}
	buf = xmalloc((size_t)st.st_size + 1);
	if (!buf) {
		rc = -ENOMEM;
		goto out;
	}
	for (p = buf, left = (size_t)st.st_size; left; ) {
		ssize_t n = read(fd, p, left);

		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0) {
			rc = n < 0 ? -errno : -EIO;
			xfree(buf, (size_t)st.st_size + 1);
			goto out;
		}
		p += n;
		left -= (size_t)n;
	}
	*(char **)out = buf;
	*len = (size_t)st.st_size;
	rc = 0;
out:
	close(fd);
	return rc;
}
