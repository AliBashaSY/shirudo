// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The hook string and load time of a loaded upatch, from its pin.
 */

#include <errno.h>
#include <limits.h>
#include <linux/bpf.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <bpf/btf.h>
#include <bpf/libbpf.h>

#include <shirudo/misc.h>
#include <shirudo/string.h>
#include <shirudo/upatch.h>

#include "malloc.h"
#include "paths.h"
#include "hex.h"
#include "upatch_resolve.h"
#include "timefmt.h"

#ifndef NFPROTO_IPV6
#define NFPROTO_IPV6			10
#endif
#ifndef NF_INET_PRE_ROUTING
#define NF_INET_PRE_ROUTING		0
#endif
#ifndef NF_IP6_PRI_RAW_BEFORE_DEFRAG
#define NF_IP6_PRI_RAW_BEFORE_DEFRAG	(-450)
#endif

int upatch_open_first_link(const char *name)
{
	char pin[PATH_MAX];
	char bpffs_name[SHIRUDO_UPATCH_BPFFS_NAME_MAX + 1];
	int fd, first_errno;

	if (upatch_bpffs_name(bpffs_name, sizeof(bpffs_name), name))
		return -ENAMETOOLONG;
	if (strsprintf(pin, sizeof(pin), "%s/%s/link",
		       SHIRUDO_BPFFS_UPATCHES_DIR, bpffs_name) < 0)
		return -ENAMETOOLONG;
	fd = bpf_obj_get(pin);
	if (fd >= 0)
		return fd;
	first_errno = errno;

	if (strsprintf(pin, sizeof(pin), "%s/%s/link_0",
		       SHIRUDO_BPFFS_UPATCHES_DIR, bpffs_name) < 0)
		return -ENAMETOOLONG;
	fd = bpf_obj_get(pin);
	if (fd >= 0)
		return fd;
	return errno == ENOENT ? -first_errno : -errno;
}

static const char *loc_prefix(__u32 prog_type, __u32 attach_type)
{
	if (prog_type == BPF_PROG_TYPE_LSM)
		return "lsm";
	if (prog_type == BPF_PROG_TYPE_NETFILTER)
		return "netfilter";
	if (prog_type == BPF_PROG_TYPE_TRACING) {
		switch (attach_type) {
		case BPF_TRACE_FENTRY:
			return "fentry";
		case BPF_TRACE_FEXIT:
			return "fexit";
		case BPF_MODIFY_RETURN:
			return "fmod_ret";
		case BPF_TRACE_RAW_TP:
			return "tp_btf";
		case BPF_TRACE_ITER:
			return "iter";
		default:
			return NULL;
		}
	}
	return NULL;
}

int upatch_resolve_location(struct btf *vbtf, const char *name,
			    char *out, size_t cap)
{
	struct bpf_link_info linfo = {};
	struct bpf_prog_info info = {};
	const char *attach_name = NULL;
	int link_fd, prog_fd, rc;
	__u32 attach_type = 0;
	const char *prefix;
	__u32 len;

	out[0] = '\0';
	link_fd = upatch_open_first_link(name);
	if (link_fd < 0)
		return link_fd;
	len = sizeof(linfo);
	rc = bpf_link_get_info_by_fd(link_fd, &linfo, &len);
	if (rc != 0 || linfo.prog_id == 0) {
		close(link_fd);
		return rc ? -errno : -EINVAL;
	}
	if (linfo.type == BPF_LINK_TYPE_UPROBE_MULTI) {
		struct bpf_link_info detail = {};
		__u32 count = linfo.uprobe_multi.count;
		__u32 path_size = linfo.uprobe_multi.path_size;
		char *path;

		if (!path_size || path_size > PATH_MAX) {
			close(link_fd);
			return -EINVAL;
		}
		path = xmalloc(path_size);
		if (!path) {
			close(link_fd);
			return -ENOMEM;
		}
		detail.uprobe_multi.path = ptr_to_u64(path);
		detail.uprobe_multi.path_size = path_size;
		len = sizeof(detail);
		rc = bpf_link_get_info_by_fd(link_fd, &detail, &len);
		close(link_fd);
		if (rc != 0) {
			rc = -errno;
			xfree(path, path_size);
			return rc;
		}
		if (!memchr(path, '\0', path_size)) {
			xfree(path, path_size);
			return -EINVAL;
		}
		snprintf(out, cap, "uprobe.multi/%s (%u funcs)", path, count);
		xfree(path, path_size);
		return 0;
	}
	close(link_fd);
	if (linfo.type == BPF_LINK_TYPE_TRACING)
		attach_type = linfo.tracing.attach_type;
	if (linfo.type == BPF_LINK_TYPE_NETFILTER &&
	    linfo.netfilter.pf == NFPROTO_IPV6 &&
	    linfo.netfilter.hooknum == NF_INET_PRE_ROUTING &&
	    linfo.netfilter.priority == NF_IP6_PRI_RAW_BEFORE_DEFRAG) {
		snprintf(out, cap, "netfilter/ipv6-pre-routing");
		return 0;
	}

	prog_fd = bpf_prog_get_fd_by_id(linfo.prog_id);
	if (prog_fd < 0)
		return -errno;
	len = sizeof(info);
	rc = bpf_prog_get_info_by_fd(prog_fd, &info, &len);
	close(prog_fd);
	if (rc != 0)
		return -errno;

	if (vbtf && info.attach_btf_id &&
	    info.attach_btf_id < btf__type_cnt(vbtf)) {
		const struct btf_type *t = btf__type_by_id(vbtf,
							   info.attach_btf_id);

		if (t)
			attach_name = btf__name_by_offset(vbtf, t->name_off);
	}

	prefix = loc_prefix(info.type, attach_type);
	if (!prefix)
		prefix = libbpf_bpf_prog_type_str(info.type);
	if (!prefix)
		prefix = "?";

	/* Strip the bpf_lsm_ prefix the kernel mangles LSM hooks with. */
	if (info.type == BPF_PROG_TYPE_LSM && attach_name &&
	    strncmp(attach_name, "bpf_lsm_", 8) == 0)
		attach_name += 8;

	if (attach_name && *attach_name)
		snprintf(out, cap, "%s/%s", prefix, attach_name);
	else
		strscpy(out, prefix, cap);
	return 0;
}

void upatch_loaded_at(const char *name, char *out, size_t cap)
{
	char dir[PATH_MAX];
	char bpffs_name[SHIRUDO_UPATCH_BPFFS_NAME_MAX + 1];
	struct stat st;

	out[0] = '\0';
	if (upatch_bpffs_name(bpffs_name, sizeof(bpffs_name), name))
		return;
	if (strsprintf(dir, sizeof(dir), "%s/%s",
		       SHIRUDO_BPFFS_UPATCHES_DIR, bpffs_name) < 0)
		return;
	if (stat(dir, &st) != 0)
		return;
	iso8601(st.st_ctim.tv_sec, out, cap);
}

void upatch_payload_sha256(const char *name, char *out, size_t cap)
{
	char dir[PATH_MAX];
	char bpffs_name[SHIRUDO_UPATCH_BPFFS_NAME_MAX + 1];
	__u8 raw[32];
	ssize_t n;

	out[0] = '\0';
	if (cap < 65)
		return;
	if (upatch_bpffs_name(bpffs_name, sizeof(bpffs_name), name))
		return;
	if (strsprintf(dir, sizeof(dir), "%s/%s",
		       SHIRUDO_BPFFS_UPATCHES_DIR, bpffs_name) < 0)
		return;
	n = getxattr(dir, SHIRUDO_XATTR_PAYLOAD, raw, sizeof(raw));
	if (n != (ssize_t)sizeof(raw))
		return;
	hex_encode(raw, sizeof(raw), out);
}
