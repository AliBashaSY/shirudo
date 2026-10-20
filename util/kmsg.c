// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * /dev/kmsg reader; the records' monotonic timestamps become wallclock
 * UTC the way `dmesg --time-format=iso` does.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "kmsg.h"
#include "timefmt.h"
#include "malloc.h"

/* Per-record buffer size accepted by /dev/kmsg: kernel allows up to 8 KiB. */
#define KMSG_READ_BUF	8192

static ssize_t read_record(int fd, char *buf, size_t cap)
{
	ssize_t n = read(fd, buf, cap);

	if (n > 0)
		return n;
	if (n == 0)
		return 0;
	if (errno == EAGAIN)
		return 0;
	if (errno == EPIPE)
		return -EPIPE;
	return -errno;
}

static int parse_record(const char *rec, size_t len,
			uint64_t *monotonic_us_out,
			const char **msg_out, size_t *msg_len_out)
{
	const char *semi, *end;
	const char *p;
	int commas = 0;

	end = rec + len;
	semi = memchr(rec, ';', len);
	if (!semi)
		return -EINVAL;

	for (p = rec; p < semi; p++) {
		if (*p == ',') {
			commas++;
			if (commas == 2) {
				uint64_t us = 0;
				const char *q = p + 1;

				while (q < semi && *q >= '0' && *q <= '9') {
					us = us * 10 + (uint64_t)(*q - '0');
					q++;
				}
				*monotonic_us_out = us;
				break;
			}
		}
	}
	if (commas < 2)
		return -EINVAL;

	*msg_out = semi + 1;
	{
		const char *nl = memchr(*msg_out, '\n',
					(size_t)(end - *msg_out));

		*msg_len_out = nl ? (size_t)(nl - *msg_out)
				  : (size_t)(end - *msg_out);
	}
	return 0;
}

/* Strip control bytes (kernel sometimes embeds \x01..\x08 markers). */
static void copy_msg(char *dst, size_t dst_cap,
		     const char *src, size_t src_len)
{
	size_t i, o = 0;

	if (dst_cap == 0)
		return;
	for (i = 0; i < src_len && o + 1 < dst_cap; i++) {
		unsigned char c = (unsigned char)src[i];

		if (c == '\t' || c == ' ' || (c >= 0x20 && c < 0x7f))
			dst[o++] = (char)c;
		else if (c >= 0x80)
			dst[o++] = (char)c;
	}
	dst[o] = '\0';
}

static void monotonic_to_iso(uint64_t mono_us,
			     const struct timespec *rt_at_open,
			     const struct timespec *mt_at_open,
			     char *out, size_t out_cap)
{
	struct timespec rec;
	int64_t delta_ns;

	/*
	 * record_realtime = realtime_at_open + (record_monotonic
	 *                                       - monotonic_at_open)
	 */
	delta_ns = (int64_t)mono_us * 1000
		 - ((int64_t)mt_at_open->tv_sec * 1000000000LL
		    + mt_at_open->tv_nsec);
	rec.tv_sec  = rt_at_open->tv_sec + delta_ns / 1000000000LL;
	rec.tv_nsec = rt_at_open->tv_nsec + delta_ns % 1000000000LL;
	if (rec.tv_nsec < 0) {
		rec.tv_nsec += 1000000000LL;
		rec.tv_sec--;
	} else if (rec.tv_nsec >= 1000000000LL) {
		rec.tv_nsec -= 1000000000LL;
		rec.tv_sec++;
	}

	if (shirudo_iso8601(rec.tv_sec, out, out_cap))
		snprintf(out, out_cap, "?");
}

int shirudo_kmsg_tail(struct shirudo_kmsg_record *out, size_t cap)
{
	struct timespec rt_at_open, mt_at_open;
	char buf[KMSG_READ_BUF];
	size_t head = 0, count = 0;
	int fd;

	if (cap == 0)
		return 0;

	fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	/*
	 * Default open() position is the *start* of the ring buffer
	 * (oldest available record). That is exactly what we want.
	 */
	if (clock_gettime(CLOCK_REALTIME, &rt_at_open) != 0 ||
	    clock_gettime(CLOCK_MONOTONIC, &mt_at_open) != 0) {
		close(fd);
		return -errno;
	}

	for (;;) {
		uint64_t mono_us = 0;
		const char *msg = NULL;
		size_t msg_len = 0;
		size_t slot;
		ssize_t n;

		n = read_record(fd, buf, sizeof(buf));
		if (n == 0)
			break;
		if (n == -EPIPE)
			continue;	/* skipped, try next */
		if (n < 0) {
			close(fd);
			return (int)n;
		}
		if (parse_record(buf, (size_t)n, &mono_us, &msg, &msg_len) != 0)
			continue;

		slot = head % cap;
		monotonic_to_iso(mono_us, &rt_at_open, &mt_at_open,
				 out[slot].ts, sizeof(out[slot].ts));
		copy_msg(out[slot].msg, sizeof(out[slot].msg), msg, msg_len);
		head++;
		if (count < cap)
			count++;
	}

	close(fd);
	/*
	 * Reorder so out[0..count) is chronological. The ring buffer
	 * may have wrapped; rotate so the oldest surviving record is
	 * at index 0.
	 */
	if (count == cap && head > cap) {
		size_t bytes = cap * sizeof(*out);
		size_t start = head % cap;
		size_t i;
		struct shirudo_kmsg_record *tmp = xmalloc(bytes);

		if (!tmp)
			return -ENOMEM;
		for (i = 0; i < cap; i++)
			tmp[i] = out[(start + i) % cap];
		for (i = 0; i < cap; i++)
			out[i] = tmp[i];
		xfree(tmp, bytes);
	}
	return (int)count;
}
