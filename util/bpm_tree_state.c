// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Target-side upatch tree state: selector matching, persisted cursor,
 * event log and the dedicated TPM projection chain.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <zlib.h>

#include <shirudo/misc.h>
#include <shirudo/string.h>

#include "bpm_tree.h"
#include "chain.h"
#include "io.h"
#include "json.h"
#include "kernel_build_id.h"
#include "kernel_release.h"
#include "log.h"
#include "malloc.h"
#include "paths.h"
#include "seal.h"

static int kv_add(struct tree_kv *dst, int *n, int max,
		  const char *key, const char *value)
{
	if (*n >= max)
		return -E2BIG;
	if (strscpy(dst[*n].key, key, sizeof(dst[0].key)) < 0 ||
	    strscpy(dst[*n].value, value, sizeof(dst[0].value)) < 0)
		return -ENOSPC;
	(*n)++;
	return 0;
}

static const char *kv_value(const struct tree_kv *v, int n,
			    const char *key)
{
	for (int i = 0; i < n; i++) {
		if (!strcmp(v[i].key, key))
			return v[i].value;
	}
	return NULL;
}

static int selector_values_match(const struct tree_selector_kv *v,
				 const char *want)
{
	if (!v || !want)
		return 0;
	for (int i = 0; i < v->n_values; i++) {
		if (!strcmp(v->values[i], want))
			return 1;
	}
	return 0;
}

bool tree_configured(void)
{
	int fd;

	if (!image_config_active())
		return access(BPM_TREE_ETC_PATH, R_OK) == 0;
	fd = image_open(IMAGE_TREE_CONF, O_RDONLY);
	if (fd < 0)
		return false;
	close(fd);
	return true;
}

/* The stream is consumed and closed. */
static int read_config_stream(FILE *f, struct tree_kv *out, int *n,
			      int max)
{
	size_t capacity = 0;
	char *line = NULL;
	int got, ret = 0;

	*n = 0;
	while ((got = read_text_line(f, &line, &capacity, 511)) != 0) {
		char *start = line, *end, *eq;

		if (got < 0) {
			ret = got;
			break;
		}

		while (*start && isspace((__u8)*start))
			start++;
		if (*start == '#' || *start == '\0')
			continue;
		end = start + strlen(start);
		while (end > start && isspace((__u8)end[-1]))
			*--end = '\0';
		eq = strchr(start, '=');
		if (!eq)
			continue;
		*eq++ = '\0';
		while (*eq && isspace((__u8)*eq))
			eq++;
		ret = kv_add(out, n, max, start, eq);
		if (ret)
			break;
	}
	libc_free(line);
	fclose(f);
	return ret;
}

static int read_config_file(const char *path, struct tree_kv *out,
			    int *n, int max)
{
	FILE *f = fopen(path, "re");

	*n = 0;
	if (!f)
		return -errno;
	return read_config_stream(f, out, n, max);
}

static int read_config_fd(int fd, struct tree_kv *out, int *n,
			  int max)
{
	FILE *f = fdopen(fd, "r");

	if (!f) {
		int ret = -errno;

		close(fd);
		return ret;
	}
	return read_config_stream(f, out, n, max);
}

/*
 * The kernel config can contain thousands of entries.  Index only the
 * selector keys, then scan the complete config once and retain matching
 * values.  This avoids both an arbitrary config-entry cap and an O(N*M)
 * lookup for N config entries and M selector keys.
 */
#define TREE_SELECTOR_HASH_SIZE 128

static __u32 selector_hash(const char *key)
{
	__u32 hash = 2166136261u;

	for (; *key; key++)
		hash = (hash ^ (__u8)*key) * 16777619u;
	return hash;
}

static void selector_hash_build(const struct tree_selector *s,
				int slots[TREE_SELECTOR_HASH_SIZE])
{
	memset(slots, 0xff, sizeof(int) * TREE_SELECTOR_HASH_SIZE);
	for (int i = 0; i < s->n_kconfig; i++) {
		__u32 slot = selector_hash(s->kconfig[i].key) &
			(TREE_SELECTOR_HASH_SIZE - 1);

		while (slots[slot] >= 0 &&
		       strcmp(s->kconfig[slots[slot]].key, s->kconfig[i].key))
			slot = (slot + 1) & (TREE_SELECTOR_HASH_SIZE - 1);
		if (slots[slot] < 0)
			slots[slot] = i;
	}
}

static int selector_hash_find(const struct tree_selector *s,
			      const int slots[TREE_SELECTOR_HASH_SIZE],
			      const char *key)
{
	__u32 slot = selector_hash(key) &
		(TREE_SELECTOR_HASH_SIZE - 1);

	for (int probes = 0; probes < TREE_SELECTOR_HASH_SIZE;
	     probes++, slot = (slot + 1) &
	     (TREE_SELECTOR_HASH_SIZE - 1)) {
		if (slots[slot] < 0)
			return -1;
		if (!strcmp(s->kconfig[slots[slot]].key, key))
			return slots[slot];
	}
	return -1;
}

static int selector_config_line(const char *line,
				const struct tree_selector *s,
				const int slots[TREE_SELECTOR_HASH_SIZE],
				char values[][TREE_STRING_MAX],
				bool found[])
{
	char copy[1024], not_set[] = "n", *start, *eq, *end, *value;
	int index;

	if (strscpy(copy, line, sizeof(copy)) < 0)
		return 0;
	start = copy;
	while (*start && isspace((__u8)*start))
		start++;
	if (!*start)
		return 0;

	if (!strncmp(start, "# CONFIG_", 9)) {
		start += 2;
		eq = strstr(start, " is not set");
		if (!eq)
			return 0;
		*eq = '\0';
		value = not_set;
	} else {
		if (*start == '#')
			return 0;
		eq = strchr(start, '=');
		if (!eq)
			return 0;
		*eq++ = '\0';
		end = start + strlen(start);
		while (end > start && isspace((__u8)end[-1]))
			*--end = '\0';
		while (*eq && isspace((__u8)*eq))
			eq++;
		end = eq + strlen(eq);
		while (end > eq && isspace((__u8)end[-1]))
			*--end = '\0';
		value = eq;
		if (value[0] == '"') {
			size_t len = strlen(value);

			if (len > 1 && value[len - 1] == '"') {
				value[len - 1] = '\0';
				value++;
			}
		}
	}

	index = selector_hash_find(s, slots, start);
	if (index < 0)
		return 0;
	if (strscpy(values[index], value, TREE_STRING_MAX) < 0)
		return -ENOSPC;
	found[index] = true;
	return 0;
}

#define KCONFIG_MAX	(4U << 20)	/* a distribution's is some 300 KiB */

/*
 * The one gzip member of the len bytes at in, with nothing behind it,
 * into *out, NUL-terminated, of *cap + 1 bytes. Not through gzgets():
 * zlib 1.3.2 reads on past the end of a member into a next one, finds
 * that cut short, and has gzclose() fail the whole file for it.
 */
static int gunzip(char *in, size_t len, char **out, size_t *out_len,
		  size_t *cap)
{
	size_t have = 0, size = 0, want;
	z_stream z = {};
	char *buf = NULL, *grown;
	int zret, ret = 0;

	if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK)
		return -ENOMEM;
	z.next_in = (Bytef *)in;
	z.avail_in = (uInt)len;
	do {
		if (have == size) {
			if (size >= KCONFIG_MAX) {
				ret = -EFBIG;
				break;
			}
			want = min(size ? size * 2 : 65536U, KCONFIG_MAX);
			grown = xreallocarray(buf, size ? size + 1 : 0, want + 1, 1);
			if (!grown) {
				ret = -ENOMEM;
				break;
			}
			buf = grown;
			size = want;
		}
		z.next_out = (Bytef *)buf + have;
		z.avail_out = (uInt)(size - have);
		zret = inflate(&z, Z_NO_FLUSH);
		have = size - z.avail_out;
		if (zret == Z_MEM_ERROR)
			ret = -ENOMEM;
		else if (zret != Z_OK && zret != Z_STREAM_END)
			ret = -EINVAL;
	} while (!ret && zret != Z_STREAM_END);
	if (!ret && z.avail_in)
		ret = -EINVAL;
	inflateEnd(&z);
	if (ret) {
		xfree(buf, size ? size + 1 : 0);
		return ret;
	}
	buf[have] = '\0';
	*out = buf;
	*out_len = have;
	*cap = size + 1;
	return 0;
}

/*
 * A line of sizeof(line) bytes or more with its newline is skipped whole,
 * as a part from the middle of a long string value would read as an
 * option of its own: its option reads as not set.
 */
static int selector_config_text(const char *text, size_t len,
				const struct tree_selector *s,
				const int slots[TREE_SELECTOR_HASH_SIZE],
				char values[][TREE_STRING_MAX], bool found[])
{
	char line[1024];
	size_t off, n;
	int ret;

	for (off = 0; off < len; off += n + 1) {
		const char *nl = memchr(text + off, '\n', len - off);

		n = nl ? (size_t)(nl - text) - off : len - off;
		if (n >= sizeof(line) - 1)
			continue;
		memcpy(line, text + off, n);
		line[n] = '\0';
		ret = selector_config_line(line, s, slots, values, found);
		if (ret)
			return ret;
	}
	return 0;
}

static int read_selector_config_file(const char *path,
				     const struct tree_selector *s,
				     const int slots[TREE_SELECTOR_HASH_SIZE],
				     char values[][TREE_STRING_MAX],
				     bool found[], bool gz)
{
	size_t len, text_len, cap;
	char *raw, *text;
	int ret;

	ret = read_file(path, KCONFIG_MAX, &raw, &len);
	if (ret)
		return ret;
	if (gz) {
		ret = gunzip(raw, len, &text, &text_len, &cap);
		xfree(raw, len + 1);
		if (ret)
			return ret;
	} else {
		text = raw;
		text_len = len;
		cap = len + 1;
	}
	ret = selector_config_text(text, text_len, s, slots, values, found);
	xfree(text, cap);
	return ret;
}

int tree_selector_matches(const struct tree_selector *s)
{
	struct utsname u;
	struct tree_kv labels[TREE_MAX_LABELS];
	char cfg_values[TREE_MAX_KCONFIG][TREE_STRING_MAX];
	int cfg_slots[TREE_SELECTOR_HASH_SIZE];
	bool cfg_found[TREE_MAX_KCONFIG] = {};
	int nl = 0, config_rc, ret;
	const char *v;
	char path[PATH_MAX];
	bool matched, config_any;

	/*
	 * An empty selector is the tree's explicit wildcard.  Evaluate this
	 * before consulting node metadata so an unconstrained operation is
	 * still applicable if uname(2) is unavailable.
	 */
	if (!s)
		return 0;
	if (!s->n_kernel_release_prefix && !s->n_kernel_build_id && !s->n_architecture &&
	    !s->n_kconfig && !s->n_labels)
		return 1;
	if (uname(&u) != 0)
		return 0;
	if (s->n_kernel_release_prefix) {
		matched = false;
		for (int i = 0; i < s->n_kernel_release_prefix; i++) {
			if (kernel_release_prefix_matches(u.release,
							  s->kernel_release_prefix[i])) {
				matched = true;
				break;
			}
		}
		if (!matched)
			return 0;
	}
	if (s->n_kernel_build_id) {
		char build_id[KERNEL_BUILD_ID_STRING_MAX];

		/*
		 * A selector carrying a build ID is deliberately fail-closed
		 * when the running kernel does not publish a GNU build ID.
		 */
		if (kernel_build_id_read(build_id))
			return 0;
		matched = false;
		for (int i = 0; i < s->n_kernel_build_id; i++) {
			if (!strcmp(build_id, s->kernel_build_id[i])) {
				matched = true;
				break;
			}
		}
		if (!matched)
			return 0;
	}
	if (s->n_architecture) {
		matched = false;
		for (int i = 0; i < s->n_architecture; i++) {
			if (!strcmp(u.machine, s->architecture[i])) {
				matched = true;
				break;
			}
		}
		if (!matched)
			return 0;
	}
	if (s->n_kconfig) {
		selector_hash_build(s, cfg_slots);
		config_rc = read_selector_config_file("/proc/config.gz", s,
						      cfg_slots, cfg_values, cfg_found,
						      true);
		config_any = false;
		for (int i = 0; i < s->n_kconfig; i++) {
			if (cfg_found[i])
				config_any = true;
		}
		if (config_rc || !config_any) {
			memset(cfg_found, 0, sizeof(cfg_found));
			if (strsprintf(path, sizeof(path), "/boot/config-%s",
				       u.release) < 0)
				return -ENAMETOOLONG;
			ret = read_selector_config_file(path, s, cfg_slots,
							cfg_values, cfg_found,
							false);
			/*
			 * No config to show, or a value longer than any
			 * selector names, matches nothing; a config that
			 * cannot be read is no answer, and the sync must not
			 * move past the operation on it.
			 */
			if (ret == -ENOENT || ret == -ENOSPC)
				return config_rc && config_rc != -ENOENT &&
				       config_rc != -ENOSPC ? config_rc : 0;
			if (ret)
				return ret;
		}
		/*
		 * The config is read into the first entry of a key, so a key
		 * named twice is matched there: left unfound, the operation
		 * was skipped and the tree moved past it for good.
		 */
		for (int i = 0; i < s->n_kconfig; i++) {
			int k = selector_hash_find(s, cfg_slots,
						   s->kconfig[i].key);

			if (k < 0 || !cfg_found[k] ||
			    !selector_values_match(&s->kconfig[i], cfg_values[k]))
				return 0;
		}
	}
	if (s->n_labels) {
		if (image_config_active()) {
			int fd = image_open(IMAGE_NODE_LABELS,
					    O_RDONLY);

			if (fd == -ENOENT)
				return 0;
			if (fd < 0)
				return fd;
			ret = read_config_fd(fd, labels, &nl, ARRAY_SIZE(labels));
		} else {
			ret = read_config_file("/etc/shirudo/node.labels",
					       labels, &nl, ARRAY_SIZE(labels));
		}
		if (ret == -ENOENT)
			return 0;
		if (ret)
			return ret;
		for (int i = 0; i < s->n_labels; i++) {
			v = kv_value(labels, nl, s->labels[i].key);
			if (!v || !selector_values_match(&s->labels[i], v))
				return 0;
		}
	}
	return 1;
}

static int tree_read_file(const char *path, __u8 **body, size_t *len)
{
	return read_file(path, JSON_MAX_BYTES, body, len);
}

/* hex is exactly 64 hex digits: a SHA-256 digest. */
static bool hex32_valid(const char *hex)
{
	__u8 raw[32];

	return !hex_decode(hex, raw, sizeof(raw));
}

int tree_pending_parse(const char *body, size_t len,
		       struct tree_pending *out)
{
	__s64 schema = 0;
	struct json_field fields[] = {
		{ "schema", JSON_T_INT, &schema, 0, 0, NULL, 0 },
		{ "seq", JSON_T_INT, &out->seq, 0, 0, NULL, 0 },
		{ "id", JSON_T_STRING, out->id, sizeof(out->id), 0, NULL, 0 },
		{ "operation_digest", JSON_T_STRING, out->operation_digest,
		  sizeof(out->operation_digest), 0, NULL, 0 },
	};
	__u8 digest[32];
	int ret;

	*out = (struct tree_pending){};
	ret = json_parse(body, len, fields, ARRAY_SIZE(fields));
	if (!ret && (schema != 1 || out->seq <= 0 ||
		     !upatch_name_valid(out->id) ||
		     sha256_digest_decode(out->operation_digest, digest)))
		ret = -EINVAL;
	return ret;
}

int tree_pending_read(struct tree_pending *out)
{
	size_t len;
	char *body;
	int ret;

	ret = read_file(BPM_TREE_PENDING_PATH, 64u * 1024u, &body, &len);
	if (ret)
		return ret;
	ret = tree_pending_parse(body, len, out);
	xfree(body, len + 1);
	return ret;
}

int tree_pending_write(const struct tree_operation *op)
{
	char buf[1024];
	struct json_emit e = { .buf = buf, .cap = sizeof(buf) };
	int ret;

	json_obj_open(&e);
	json_kv_int(&e, "schema", 1);
	json_kv_int(&e, "seq", op->seq);
	json_kv_string(&e, "id", op->id);
	json_kv_string(&e, "operation_digest", op->operation_digest);
	json_obj_close(&e);
	if (e.err)
		return e.err;
	ret = seal_mkdir_p(BPM_TREE_VAR_DIR, 0700);
	if (ret)
		return ret;
	return seal_write_file(BPM_TREE_PENDING_PATH, buf, e.len, 0600);
}

int tree_state_read(struct tree_state *out)
{
	__u8 *body, digest[32];
	size_t len;
	__s64 schema = 0;
	struct json_field f[] = {
		{ "schema", JSON_T_INT, &schema, 0, 0, NULL, 0 },
		{ "tree_id", JSON_T_STRING, out->tree_id,
		  sizeof(out->tree_id), 0, NULL, 0 },
		{ "generation", JSON_T_INT, &out->generation, 0, 0, NULL, 0 },
		{ "last_seq", JSON_T_INT, &out->last_seq, 0, 0, NULL, 0 },
		{ "projection_root", JSON_T_STRING, out->projection_root,
		  sizeof(out->projection_root), 0, NULL, 0 },
		{ "projection_root_tpm", JSON_T_STRING,
		  out->projection_root_tpm, sizeof(out->projection_root_tpm), 0, NULL, 0 },
		{ "manifest_digest", JSON_T_STRING, out->manifest_digest,
		  sizeof(out->manifest_digest), 0, NULL, 0 },
	};
	int ret;

	*out = (struct tree_state){};
	ret = tree_read_file(BPM_TREE_STATE_PATH, &body, &len);
	if (ret)
		return ret;
	ret = json_parse((const char *)body, len, f, ARRAY_SIZE(f));
	if (!ret && (schema != 1 || !f[1].seen || !f[3].seen || !f[4].seen))
		ret = -EINVAL;
	if (!ret && !f[5].seen) {
		/* State written before the software projection field was added. */
		memcpy(out->projection_root_tpm, out->projection_root,
		       sizeof(out->projection_root_tpm));
	}
	if (!ret && (strlen(out->projection_root) != 64 ||
		     !hex32_valid(out->projection_root) ||
		     (out->projection_root_tpm[0] &&
		      (strlen(out->projection_root_tpm) != 64 ||
		       !hex32_valid(out->projection_root_tpm)))))
		ret = -EINVAL;
	if (!ret && out->manifest_digest[0] &&
	    sha256_digest_decode(out->manifest_digest, digest))
		ret = -EINVAL;
	out->schema = (int)schema;
	xfree(body, len + 1);
	return ret;
}

static int tree_atomic_write(const char *path, const void *data, size_t len)
{
	const char *slash = strrchr(path, '/');
	char dir[PATH_MAX];
	int ret;

	if (!slash || (size_t)(slash - path) >= sizeof(dir))
		return -EINVAL;
	memcpy(dir, path, slash - path);
	dir[slash - path] = '\0';
	ret = seal_mkdir_p(dir, 0700);
	if (ret) {
		pr_err("tree state mkdir %s: %s", dir, strerror(-ret));
		return ret;
	}
	return seal_write_file(path, data, len, 0600);
}

int tree_state_write(const struct tree_state *s)
{
	char buf[4096];
	struct json_emit e = { .buf = buf, .cap = sizeof(buf) };

	json_obj_open(&e);
	json_kv_int(&e, "schema", 1);
	json_kv_string(&e, "tree_id", s->tree_id);
	json_kv_int(&e, "generation", s->generation);
	json_kv_int(&e, "last_seq", s->last_seq);
	json_kv_string(&e, "projection_root", s->projection_root);
	json_kv_string(&e, "projection_root_tpm", s->projection_root_tpm);
	json_kv_string(&e, "manifest_digest", s->manifest_digest);
	json_obj_close(&e);
	if (e.err)
		return e.err;
	return tree_atomic_write(BPM_TREE_STATE_PATH, buf, e.len);
}

int tree_event_append(__s64 seq, const char *operation_digest,
		      const __u8 projection_root[32])
{
	char buf[1024], root[65];
	struct json_emit e = { .buf = buf, .cap = sizeof(buf) };
	__u8 digest[32];
	int ret;

	if (sha256_digest_decode(operation_digest, digest))
		return -EINVAL;
	hex_encode(projection_root, 32, root);
	json_obj_open(&e);
	json_kv_int(&e, "schema", 1);
	json_kv_int(&e, "seq", seq);
	json_kv_string(&e, "digest", operation_digest);
	json_kv_string(&e, "projection_root", root);
	json_obj_close(&e);
	if (e.err)
		return e.err;
	ret = seal_mkdir_p(BPM_TREE_VAR_DIR, 0700);
	if (ret)
		return ret;
	return seal_append_line(BPM_TREE_EVENTLOG_PATH, buf, e.len);
}

int tree_event_replay(__u8 out[32])
{
	__u8 *body;
	size_t len;
	char *save, *line;
	__u8 chain[32] = {};
	__s64 previous_seq = 0;
	int ret = tree_read_file(BPM_TREE_EVENTLOG_PATH, &body, &len);

	if (ret)
		return ret;
	/* A NUL would end the walk early and replay a shorter chain. */
	if (memchr(body, '\0', len)) {
		xfree(body, len + 1);
		return -EINVAL;
	}
	body[lines_len((const char *)body, len)] = '\0';
	for (line = strtok_r((char *)body, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char digest[128] = {};
		char projection[65] = {};
		__s64 seq = 0;
		struct json_field f[] = {
			{ "seq", JSON_T_INT, &seq, 0, 0, NULL, 0 },
			{ "digest", JSON_T_STRING, digest, sizeof(digest),
			  0, NULL, 0 },
			{ "projection_root", JSON_T_STRING, projection,
			  sizeof(projection), 0, NULL, 0 },
		};
		__u8 d[32], next[64], recorded[32];

		ret = json_parse(line, strlen(line), f, ARRAY_SIZE(f));
		if (ret || !f[0].seen || !f[1].seen || !f[2].seen ||
		    seq <= previous_seq || strlen(projection) != 64 ||
		    hex_decode(projection, recorded, 32)) {
			ret = -EINVAL;
			break;
		}
		previous_seq = seq;
		if (sha256_digest_decode(digest, d)) {
			ret = -EINVAL;
			break;
		}
		memcpy(next, chain, 32);
		memcpy(next + 32, d, 32);
		ret = sha256_calc(next, sizeof(next), chain);
		if (ret)
			break;
		if (memneq_explicit(chain, recorded, 32)) {
			ret = -EINVAL;
			break;
		}
	}
	if (!ret)
		memcpy(out, chain, 32);
	xfree(body, len + 1);
	return ret;
}

void tree_emit_status(struct json_emit *e)
{
	struct tree_state s = {};
	__u8 live[32], replay[32];
	char hex[65];
	bool configured = tree_configured();

	json_obj_open_key(e, "upatch_tree");
	json_kv_bool(e, "configured", configured);
	json_kv_bool(e, "pending", access(BPM_TREE_PENDING_PATH,
					  R_OK) == 0);
	if (tree_state_read(&s) == 0) {
		json_kv_string(e, "tree_id", s.tree_id);
		json_kv_int(e, "generation", s.generation);
		json_kv_int(e, "last_seq", s.last_seq);
		json_kv_string(e, "projection_root", s.projection_root);
		json_kv_string(e, "manifest_digest", s.manifest_digest);
	} else {
		json_kv_null(e, "state");
	}
	if (tree_event_replay(replay) == 0) {
		hex_encode(replay, 32, hex);
		json_kv_string(e, "eventlog_projection_root", hex);
		json_kv_bool(e, "eventlog_matches_state",
			     hex_decode(s.projection_root, live,
					32) == 0 &&
			     !memneq_explicit(live, replay, 32));
	} else {
		json_kv_null(e, "eventlog_projection_root");
		json_kv_null(e, "eventlog_matches_state");
	}
	json_obj_close(e);
}
