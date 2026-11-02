// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Read back the identity map init pinned in the private bpffs.
 */

#include <errno.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <linux/bpf.h>

#include <bpf/bpf.h>

#include <shirudo/enforcement.h>
#include <shirudo/misc.h>

#include "identity.h"
#include "paths.h"

int shirudo_enforcement_active(void)
{
	int ret = prctl(PR_GET_ENFORCEMENT, 0, 0, 0, 0);

	if (ret >= 0)
		return -EPROTO;
	if (errno == PR_ENFORCING_ERR)
		return 1;
	return errno == EINVAL || errno == ENOSYS ? 0 : -errno;
}

int identity_read(struct shirudo_identity *out)
{
	struct bpf_map_info info = {};
	__u32 len = sizeof(info);
	__u32 zero = 0;
	int fd, ret;

	memset(out, 0, sizeof(*out));
	fd = bpf_obj_get(IDENTITY_PIN);
	if (fd < 0)
		return -errno;
	ret = bpf_map_get_info_by_fd(fd, &info, &len);
	if (ret) {
		ret = -errno;
	} else if (info.value_size != sizeof(*out)) {
		ret = -EPROTO;
	} else {
		ret = bpf_map_lookup_elem(fd, &zero, out);
		if (ret)
			ret = -errno;
	}
	close(fd);
	return ret;
}
