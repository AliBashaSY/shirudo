// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The upatch tree's state.json and its eventlog, as a target reads them
 * back before a sync.
 */

#include "fuzz.h"
#include "paths.h"

static char fuzz_path[64];

#undef BPM_TREE_STATE_PATH
#define BPM_TREE_STATE_PATH fuzz_path
#undef BPM_TREE_EVENTLOG_PATH
#define BPM_TREE_EVENTLOG_PATH fuzz_path

#include "bpm_tree_state.c"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct shirudo_tree_state state;
	__u8 root[32];
	int fd;

	fd = fuzz_file(data, size, fuzz_path, sizeof(fuzz_path));
	if (fd < 0)
		return 0;
	tree_state_read(&state);
	tree_event_replay(root);
	close(fd);
	return 0;
}
