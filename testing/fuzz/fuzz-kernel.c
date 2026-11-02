// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * What the kernel hands status: a /dev/kmsg record, /proc/modules and the
 * ELF notes of /sys/kernel/notes.
 */

#include "fuzz.h"
#include "kernel_build_id.h"
#include "kmodules.h"
#include "kmsg.c"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	const struct timespec rt = { 1792540800, 999999999 };
	char build_id[KERNEL_BUILD_ID_STRING_MAX];
	const struct timespec mt = { 86400, 1 };
	char ts[KMSG_TS_LEN];
	struct kmod_set set;
	char msg[1024], *text;
	const char *m;
	__u64 us;
	size_t n;

	if (!parse_record((const char *)data, size, &us, &m, &n)) {
		monotonic_to_iso(us, &rt, &mt, ts, sizeof(ts));
		copy_msg(msg, sizeof(msg), m, n);
	}

	text = xmalloc(size + 1);
	if (!text)
		return 0;
	memcpy(text, data, size);
	text[size] = '\0';
	if (!kmodules_parse(text, &set))
		kmodules_free(&set);
	xfree(text, size + 1);

	kernel_build_id_from_notes(data, size, build_id);
	return 0;
}
