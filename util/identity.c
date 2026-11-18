// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Read back the identity map init pinned in the private bpffs.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <linux/bpf.h>

#include <bpf/bpf.h>

#include <shirudo/enforcement.h>
#include <shirudo/misc.h>

#include "identity.h"
#include "paths.h"

int enforcement_active(void)
{
	int ret = prctl(PR_GET_ENFORCEMENT, 0, 0, 0, 0);

	if (ret >= 0)
		return -EPROTO;
	if (errno == PR_ENFORCING_ERR)
		return 1;
	return errno == EINVAL || errno == ENOSYS ? 0 : -errno;
}

static int identity_map_read(int fd, struct identity *out)
{
	struct bpf_map_info info = {};
	__u32 len = sizeof(info);
	__u32 zero = 0;

	if (bpf_map_get_info_by_fd(fd, &info, &len))
		return -errno;
	if (info.value_size != sizeof(*out))
		return -EPROTO;
	if (bpf_map_lookup_elem(fd, &zero, out))
		return -errno;
	return 0;
}

int identity_read_at(int dirfd, const char *name, struct identity *out)
{
	LIBBPF_OPTS(bpf_obj_get_opts, opts,
		    .file_flags = BPF_F_PATH_FD,
		    .path_fd = dirfd);
	int fd, ret;

	memset(out, 0, sizeof(*out));
	fd = bpf_obj_get_opts(name, &opts);
	if (fd < 0)
		return -errno;
	ret = identity_map_read(fd, out);
	close(fd);
	return ret;
}

int identity_read(struct identity *out)
{
	return identity_read_at(AT_FDCWD, IDENTITY_PIN, out);
}
