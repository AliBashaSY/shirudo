/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Userspace upatch map decoders.
 */

#ifndef SHIRUDO_UTIL_UPATCH_DECODE_H
#define SHIRUDO_UTIL_UPATCH_DECODE_H

#include <stddef.h>

#include <shirudo/misc.h>

struct shirudo_map_view;

/*
 * The decoder is the signed one in the archived upatch. *out is
 * xmalloc'd; -ENOENT without a usable decoder, which an export treats
 * as failing the map.
 */
int shirudo_upatch_decode_map_json(const struct shirudo_map_view *v,
				   char **out, size_t *out_len);

/* Variant for consumers such as S3 archival with a larger output budget. */
int shirudo_upatch_decode_map_json_limit(const struct shirudo_map_view *v,
					 size_t max_bytes,
					 char **out, size_t *out_len);

int shirudo_upatch_decode_map_otel(const struct shirudo_map_view *v,
				   char **out, size_t *out_len);

/* Explicit-artifact variants used by offline consumers and tests. */
int shirudo_upatch_decode_map_file_json(const char *path,
					const struct shirudo_map_view *v,
					char **out, size_t *out_len);
int shirudo_upatch_decode_map_file_json_limit(const char *path,
					      const struct shirudo_map_view *v,
					      size_t max_bytes, char **out,
					      size_t *out_len);
int shirudo_upatch_decode_map_file_otel(const char *path,
					const struct shirudo_map_view *v,
					char **out, size_t *out_len);

int shirudo_upatch_decode_map_signal(const struct shirudo_map_view *v,
				     __u32 *signal_out);

#endif /* SHIRUDO_UTIL_UPATCH_DECODE_H */
