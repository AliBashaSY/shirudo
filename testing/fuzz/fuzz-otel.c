// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * What an OTLP collector answers, the response header lines and the
 * partial-success body, the decoded map JSON a log record carries, and
 * the configured OTEL_EXPORTER_OTLP_HEADERS.
 */

#include "fuzz.h"
#include "upatch_maps_otel.c"

/* The map decoding the exporter calls is fuzzed on its own. */
const struct btf_type *btf__type_by_id(const struct btf *btf, __u32 id)
{
	return NULL;
}

bool upatch_map_is_percpu(__u32 type)
{
	return false;
}

int upatch_decode_map_json(const struct map_view *v,
			   char **out, size_t *out_len)
{
	return -EOPNOTSUPP;
}

int upatch_decode_map_json_limit(const struct map_view *v,
				 size_t max_bytes,
				 char **out, size_t *out_len)
{
	return -EOPNOTSUPP;
}

int upatch_decode_map_otel(const struct map_view *v,
			   char **out, size_t *out_len)
{
	return -EOPNOTSUPP;
}

int upatch_decode_map_file_json(const char *path,
				const struct map_view *v,
				char **out, size_t *out_len)
{
	return -EOPNOTSUPP;
}

int upatch_decode_map_file_json_limit(const char *path,
				      const struct map_view *v,
				      size_t max_bytes, char **out,
				      size_t *out_len)
{
	return -EOPNOTSUPP;
}

int upatch_decode_map_file_otel(const char *path,
				const struct map_view *v,
				char **out, size_t *out_len)
{
	return -EOPNOTSUPP;
}

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct json_emit e = {};
	struct otel_response r = {};
	struct otel_state st = {};
	size_t off, n, cap;
	char *body, *line;

	/*
	 * curl hands the header callback one line at a time, end included
	 * and not NUL-terminated: each line in a buffer of its own size.
	 */
	for (off = 0; off < size; off += n) {
		const __u8 *nl = memchr(data + off, '\n', size - off);

		n = nl ? (size_t)(nl - data) - off + 1 : size - off;
		line = xmalloc(n);
		if (!line)
			return 0;
		memcpy(line, data + off, n);
		response_header(line, 1, n, &r);
		xfree(line, n);
	}

	body = xmalloc(size + 1);
	if (!body)
		return 0;
	memcpy(body, data, size);
	body[size] = '\0';
	check_partial_success(OTEL_LOG, body, size);
	check_partial_success(OTEL_METRIC, body, size);

	cap = generic_log_capacity(size);
	e.buf = xmalloc(cap);
	if (e.buf) {
		e.cap = cap;
		json_obj_open(&e);
		json_anyvalue_from_document(&e, body, size, false);
		xfree(e.buf, cap);
	}

	if (!memchr(data, '\0', size)) {
		add_configured_headers(&st, body);
		curl_slist_free_all(st.headers);
	}
	xfree(body, size + 1);
	return 0;
}
