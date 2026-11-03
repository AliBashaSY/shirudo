// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/xattr.h>

#include <shirudo/misc.h>
#include <shirudo/string.h>
#include <shirudo/upatch.h>

#include "eventlog.h"
#include "hex.h"
#include "malloc.h"
#include "measured.h"
#include "paths.h"
#include "seal.h"
#include "upatch.h"

#define MEASURED_MAX	4096

struct measured_entry {
	char	name[UPATCH_NAME_MAX + 1];
	char	payload_hex[2 * UPATCH_PAYLOAD_HASH_LEN + 1];
};

struct measured_set {
	struct measured_entry	*entries;
	size_t			n;
};

static int measured_event(enum eventlog_upatch_event event, const char *name,
			  const char *payload_hex, void *ctx)
{
	struct measured_set *s = ctx;
	size_t i;

	if (event == EVENTLOG_UPATCH_INIT) {
		s->n = 0;
		return 0;
	}
	for (i = 0; i < s->n; i++) {
		if (!strcmp(s->entries[i].name, name))
			break;
	}
	if (event == EVENTLOG_UPATCH_UNLOAD) {
		if (i < s->n)
			s->entries[i] = s->entries[--s->n];
		return 0;
	}
	if (i == s->n) {
		if (s->n == MEASURED_MAX)
			return -ENOSPC;
		s->n++;
	}
	if (strscpy(s->entries[i].name, name,
		    sizeof(s->entries[i].name)) < 0 ||
	    strscpy(s->entries[i].payload_hex, payload_hex,
		    sizeof(s->entries[i].payload_hex)) < 0)
		return -EINVAL;
	return 0;
}

static int measured_set_fold(struct measured_set **out, bool all)
{
	struct measured_set *s;
	int ret;

	*out = NULL;
	s = xmalloc(sizeof(*s));
	if (!s)
		return -ENOMEM;
	s->n = 0;
	s->entries = xcalloc(MEASURED_MAX, sizeof(*s->entries));
	if (!s->entries) {
		xfree(s, sizeof(*s));
		return -ENOMEM;
	}
	ret = all ? eventlog_upatches(measured_event, s) :
	      eventlog_boot_upatches(measured_event, s);
	if (ret == -ENOENT && all)
		ret = 0;
	if (ret) {
		measured_set_free(s);
		return ret;
	}
	*out = s;
	return 0;
}

int measured_set_boot(struct measured_set **out)
{
	return measured_set_fold(out, false);
}

int measured_set_log(struct measured_set **out)
{
	return measured_set_fold(out, true);
}

bool measured_holds(const struct measured_set *s, const char *name,
		    const char *payload_hex)
{
	size_t i;

	for (i = 0; i < s->n; i++) {
		if (!strcmp(s->entries[i].name, name) &&
		    !strcmp(s->entries[i].payload_hex, payload_hex))
			return true;
	}
	return false;
}

void measured_set_free(struct measured_set *s)
{
	if (!s)
		return;
	xfree(s->entries, MEASURED_MAX * sizeof(*s->entries));
	xfree(s, sizeof(*s));
}

static void free_names(struct dirent **names, int n)
{
	int i;

	for (i = 0; i < n; i++)
		libc_free(names[i]);
	libc_free(names);
}

int measured_unmeasured_pins(const struct measured_set *s,
			     int (*cb)(const char *name, void *ctx),
			     void *ctx)
{
	struct dirent **pins = NULL;
	int n, i, ret = 0, unmeasured = 0;

	n = scandir(BPFFS_UPATCHES_DIR, &pins, NULL, alphasort);
	if (n < 0)
		return errno == ENOENT ? 0 : -errno;
	for (i = 0; i < n; i++) {
		char payload_hex[2 * UPATCH_PAYLOAD_HASH_LEN + 1] = "";
		__u8 payload[UPATCH_PAYLOAD_HASH_LEN];
		char name[UPATCH_NAME_MAX + 1];
		char dir[PATH_MAX];

		if (!strcmp(pins[i]->d_name, ".") ||
		    !strcmp(pins[i]->d_name, "..") ||
		    upatch_name_from_bpffs(name, sizeof(name),
					   pins[i]->d_name) ||
		    strsprintf(dir, sizeof(dir), "%s/%s", BPFFS_UPATCHES_DIR,
			       pins[i]->d_name) < 0)
			continue;
		if (getxattr(dir, XATTR_PAYLOAD, payload, sizeof(payload)) ==
		    (ssize_t)sizeof(payload))
			hex_encode(payload, sizeof(payload), payload_hex);
		if (measured_holds(s, name, payload_hex))
			continue;
		unmeasured++;
		if (cb) {
			ret = cb(name, ctx);
			if (ret)
				break;
		}
	}
	free_names(pins, n);
	return ret ? ret : unmeasured;
}

int measured_unmeasured_archives(const struct measured_set *s,
				 int (*cb)(const char *name,
					   const char *path, void *ctx),
				 void *ctx)
{
	struct dirent **names = NULL;
	int n, i, ret = 0, unmeasured = 0;

	n = scandir(UPATCHES_DIR, &names, upatch_select, alphasort);
	if (n < 0)
		return errno == ENOENT ? 0 : -errno;
	for (i = 0; i < n; i++) {
		char payload_hex[2 * UPATCH_PAYLOAD_HASH_LEN + 1];
		char name[UPATCH_NAME_MAX + 1];
		char path[PATH_MAX];
		struct upatch u;

		if (strsprintf(path, sizeof(path), "%s/%s", UPATCHES_DIR,
			       names[i]->d_name) < 0 ||
		    !seal_owns(path) || upatch_open(path, &u))
			continue;
		memcpy(name, u.hdr.name, UPATCH_NAME_MAX);
		name[UPATCH_NAME_MAX] = '\0';
		hex_encode(u.hdr.payload_sha256, UPATCH_PAYLOAD_HASH_LEN,
			   payload_hex);
		upatch_close(&u);
		if (measured_holds(s, name, payload_hex))
			continue;
		unmeasured++;
		if (cb) {
			ret = cb(name, path, ctx);
			if (ret)
				break;
		}
	}
	free_names(names, n);
	return ret ? ret : unmeasured;
}
