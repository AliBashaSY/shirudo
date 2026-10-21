// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * /proc/modules parser and dependency-tree printer.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <time.h>

#include <shirudo/misc.h>

#include "format.h"
#include "kmodules.h"
#include "malloc.h"
#include "timefmt.h"

static int slurp(char **buf_out, size_t *len_out)
{
	size_t cap = 16384, len = 0;
	char *buf;
	int fd;

	fd = open("/proc/modules", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	buf = xmalloc(cap);
	if (!buf) {
		close(fd);
		return -ENOMEM;
	}
	for (;;) {
		ssize_t n = read(fd, buf + len, cap - len - 1);

		if (n < 0) {
			int e = -errno;

			xfree(buf, cap);
			close(fd);
			return e;
		}
		if (n == 0)
			break;
		len += (size_t)n;
		if (cap - len < 4096) {
			void *p = xreallocarray(buf, cap, cap * 2, 1);

			if (!p) {
				xfree(buf, cap);
				close(fd);
				return -ENOMEM;
			}
			buf = p;
			cap *= 2;
		}
	}
	close(fd);
	buf[len] = '\0';
	*buf_out = buf;
	*len_out = cap;	/* freed at full cap by caller */
	return 0;
}

static int kmod_cmp(const void *a, const void *b)
{
	return strcmp(((const struct shirudo_kmod *)a)->name,
		      ((const struct shirudo_kmod *)b)->name);
}

static int kmod_find(const struct shirudo_kmod_set *set, const char *name)
{
	size_t lo = 0, hi = set->n;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		int c = strcmp(set->mods[mid].name, name);

		if (c == 0)
			return (int)mid;
		if (c < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return -1;
}

int shirudo_kmodules_read(struct shirudo_kmod_set *set)
{
	size_t mod_cap = 64, deps_cap = 256, edge_cap = 256, n_edges = 0;
	struct edge { __u16 user, dep; } *edges = NULL;
	char *buf = NULL, *line, *end;
	size_t buf_cap = 0, i, j;
	int rc;

	memset(set, 0, sizeof(*set));
	rc = slurp(&buf, &buf_cap);
	if (rc)
		return rc;
	end = buf + strlen(buf);
	for (line = buf; line < end; line++) {
		if (*line == '\n')
			*line = '\0';
	}

	set->mods = xmalloc(mod_cap * sizeof(*set->mods));
	set->deps = xmalloc(deps_cap * sizeof(*set->deps));
	edges = xmalloc(edge_cap * sizeof(*edges));
	if (!set->mods || !set->deps || !edges) {
		rc = -ENOMEM;
		goto out;
	}

	/*
	 * Names first, sorted, so that the second pass can turn the
	 * used-by column into indices.
	 */
	for (line = buf; line < end; line += strlen(line) + 1) {
		struct shirudo_kmod *m;
		char *p = line, *e;
		size_t name_len;

		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			continue;
		e = p;
		while (*e && *e != ' ' && *e != '\t')
			e++;
		name_len = (size_t)(e - p);
		if (name_len == 0 || name_len >= SHIRUDO_KMOD_NAME_LEN)
			continue;
		if (set->n == mod_cap) {
			void *np = xreallocarray(set->mods, mod_cap,
						 mod_cap * 2,
						 sizeof(*set->mods));

			if (!np) {
				rc = -ENOMEM;
				goto out;
			}
			set->mods = np;
			mod_cap *= 2;
		}
		m = &set->mods[set->n++];
		memcpy(m->name, p, name_len);
		m->name[name_len] = '\0';
		m->size = strtoull(e, &p, 10);
		m->refcount = (__u32)strtoul(p, NULL, 10);
	}
	qsort(set->mods, set->n, sizeof(*set->mods), kmod_cmp);

	/*
	 * The fourth column of a line names who uses the module, so each
	 * entry there is an edge from that user down to the module.
	 */
	for (line = buf; line < end; line += strlen(line) + 1) {
		char name[SHIRUDO_KMOD_NAME_LEN];
		char *p = line, *e;
		size_t name_len;
		int dep, col;

		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			continue;
		e = p;
		while (*e && *e != ' ' && *e != '\t')
			e++;
		name_len = (size_t)(e - p);
		if (name_len == 0 || name_len >= sizeof(name))
			continue;
		memcpy(name, p, name_len);
		name[name_len] = '\0';
		dep = kmod_find(set, name);
		if (dep < 0)
			continue;
		p = e;
		for (col = 0; col < 2 && *p; col++) {
			while (*p == ' ' || *p == '\t')
				p++;
			while (*p && *p != ' ' && *p != '\t')
				p++;
		}
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '-')
			continue;
		while (*p && *p != ' ' && *p != '\t') {
			int user = -1;

			e = p;
			while (*e && *e != ',' && *e != ' ' && *e != '\t')
				e++;
			name_len = (size_t)(e - p);
			if (name_len && name_len < sizeof(name)) {
				memcpy(name, p, name_len);
				name[name_len] = '\0';
				user = kmod_find(set, name);
			}
			p = *e == ',' ? e + 1 : e;
			if (user < 0)
				continue;
			if (n_edges == edge_cap) {
				void *np = xreallocarray(edges, edge_cap,
							 edge_cap * 2,
							 sizeof(*edges));

				if (!np) {
					rc = -ENOMEM;
					goto out;
				}
				edges = np;
				edge_cap *= 2;
			}
			edges[n_edges].user = (__u16)user;
			edges[n_edges].dep = (__u16)dep;
			n_edges++;
		}
	}

	for (i = 0; i < set->n; i++) {
		set->mods[i].deps_off = (__u16)set->n_deps;
		set->mods[i].n_deps = 0;
		for (j = 0; j < n_edges; j++) {
			if (edges[j].user != i)
				continue;
			if (set->n_deps == deps_cap) {
				void *np = xreallocarray(set->deps, deps_cap,
							 deps_cap * 2,
							 sizeof(*set->deps));

				if (!np) {
					rc = -ENOMEM;
					goto out;
				}
				set->deps = np;
				deps_cap *= 2;
			}
			set->deps[set->n_deps++] = edges[j].dep;
			set->mods[i].n_deps++;
		}
	}

	/*
	 * Stat each /sys/module/<name>/ for its kernfs mtime, which the
	 * kernel sets when sysfs_create_dir registers the module's
	 * kobject during init_module/finit_module. That mtime is the
	 * load time. ENOENT on a race (rmmod between /proc/modules read
	 * and this stat) is non-fatal; leave loaded_at zero.
	 */
	for (i = 0; i < set->n; i++) {
		struct shirudo_kmod *m = &set->mods[i];
		char path[64 + sizeof("/sys/module/")];
		struct stat st;

		snprintf(path, sizeof(path), "/sys/module/%s", m->name);
		if (stat(path, &st) == 0) {
			m->loaded_at_sec  = (__s64)st.st_mtim.tv_sec;
			m->loaded_at_nsec = (__s32)st.st_mtim.tv_nsec;
		}
	}
	rc = 0;
out:
	xfree(edges, edge_cap * sizeof(*edges));
	xfree(buf, buf_cap);
	if (rc) {
		xfree(set->mods, mod_cap * sizeof(*set->mods));
		xfree(set->deps, deps_cap * sizeof(*set->deps));
		memset(set, 0, sizeof(*set));
	}
	return rc;
}

void shirudo_kmodules_free(struct shirudo_kmod_set *set)
{
	xfree(set->mods, set->n * sizeof(*set->mods));
	xfree(set->deps, set->n_deps * sizeof(*set->deps));
	memset(set, 0, sizeof(*set));
}

static void print_subtree(const struct shirudo_kmod_set *set, size_t i,
			  const char *prefix, int is_last, __u8 *seen)
{
	const struct shirudo_kmod *m = &set->mods[i];
	const char *branch = is_last ? "└── " : "├── ";
	int already = (seen[i >> 3] >> (i & 7)) & 1;
	char size_s[16];
	char child_prefix[256];

	shirudo_human_size(m->size, size_s, sizeof(size_s));

	if (already) {
		printf("%s%s%s (shared)\n", prefix, branch, m->name);
		return;
	}
	seen[i >> 3] |= (__u8)(1U << (i & 7));

	printf("%s%s%s (%s, refs=%u)\n", prefix, branch, m->name,
	       size_s, (__u32)m->refcount);

	snprintf(child_prefix, sizeof(child_prefix), "%s%s", prefix,
		 is_last ? "    " : "│   ");

	for (__u16 k = 0; k < m->n_deps; k++) {
		size_t dep = set->deps[m->deps_off + k];
		int last_child = (k == m->n_deps - 1);

		print_subtree(set, dep, child_prefix, last_child, seen);
	}
}

void shirudo_kmodules_print_tree(const struct shirudo_kmod_set *set)
{
	__u8 *used_by;
	__u8 *seen;
	size_t bitmap_bytes;
	size_t n_roots = 0;
	size_t *roots;

	if (set->n == 0) {
		printf("  (no modules loaded)\n");
		return;
	}

	bitmap_bytes = (set->n + 7) / 8;
	used_by = xcalloc(bitmap_bytes, 1);
	seen   = xcalloc(bitmap_bytes, 1);
	roots  = xmalloc(set->n * sizeof(*roots));
	if (!used_by || !seen || !roots) {
		xfree(used_by, bitmap_bytes);
		xfree(seen, bitmap_bytes);
		xfree(roots, set->n * sizeof(*roots));
		printf("  (out of memory)\n");
		return;
	}

	/*
	 * Mark "is a dep of someone": that module is NOT a root.
	 * The remainder are entry-point modules nothing else uses.
	 */
	for (size_t i = 0; i < set->n; i++) {
		const struct shirudo_kmod *m = &set->mods[i];

		for (__u16 k = 0; k < m->n_deps; k++) {
			size_t d = set->deps[m->deps_off + k];

			used_by[d >> 3] |= (__u8)(1U << (d & 7));
		}
	}
	for (size_t i = 0; i < set->n; i++) {
		if (((used_by[i >> 3] >> (i & 7)) & 1) == 0)
			roots[n_roots++] = i;
	}

	for (size_t r = 0; r < n_roots; r++) {
		int is_last = (r == n_roots - 1);

		print_subtree(set, roots[r], "  ", is_last, seen);
	}

	xfree(used_by, bitmap_bytes);
	xfree(seen, bitmap_bytes);
	xfree(roots, set->n * sizeof(*roots));
}

/*
 * qsort comparator: descending by (loaded_at_sec, loaded_at_nsec).
 * Indices into set->mods[]. Modules with loaded_at_sec == 0 (stat
 * failure) sort to the bottom so they don't crowd out real entries.
 */
struct recent_idx {
	const struct shirudo_kmod_set	*set;
	size_t				idx;
};

static int recent_cmp(const void *a, const void *b)
{
	const struct recent_idx *x = a, *y = b;
	const struct shirudo_kmod *mx = &x->set->mods[x->idx];
	const struct shirudo_kmod *my = &y->set->mods[y->idx];

	if (mx->loaded_at_sec == 0 && my->loaded_at_sec != 0)
		return 1;
	if (mx->loaded_at_sec != 0 && my->loaded_at_sec == 0)
		return -1;
	if (mx->loaded_at_sec != my->loaded_at_sec)
		return mx->loaded_at_sec < my->loaded_at_sec ? 1 : -1;
	if (mx->loaded_at_nsec != my->loaded_at_nsec)
		return mx->loaded_at_nsec < my->loaded_at_nsec ? 1 : -1;
	return strcmp(mx->name, my->name);
}

void shirudo_kmodules_print_recent(const struct shirudo_kmod_set *set,
				   size_t last_n, __s64 boot_sec)
{
	struct recent_idx *all;
	size_t take, i;

	if (set->n == 0) {
		printf("  (no modules loaded)\n");
		return;
	}

	all = xmalloc(set->n * sizeof(*all));
	if (!all) {
		printf("  (out of memory)\n");
		return;
	}
	for (i = 0; i < set->n; i++) {
		all[i].set = set;
		all[i].idx = i;
	}
	qsort(all, set->n, sizeof(*all), recent_cmp);
	take = set->n < last_n ? set->n : last_n;
	for (i = 0; i < take; i++) {
		const struct shirudo_kmod *m = &set->mods[all[i].idx];
		char ts[32], size_s[16], age[24];

		if (m->loaded_at_sec <= 0 ||
		    shirudo_iso8601((time_t)m->loaded_at_sec, ts, sizeof(ts)))
			snprintf(ts, sizeof(ts), "?");
		shirudo_human_size(m->size, size_s, sizeof(size_s));
		if (boot_sec > 0 && m->loaded_at_sec >= boot_sec)
			shirudo_format_duration((long)(m->loaded_at_sec - boot_sec),
						age, sizeof(age));
		else
			snprintf(age, sizeof(age), "?");

		printf("  [%s]  %s (%s, refs=%u, %s since boot)\n",
		       ts, m->name, size_s, (__u32)m->refcount, age);
	}
	xfree(all, set->n * sizeof(*all));
}
