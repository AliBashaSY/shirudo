/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * .upatch container parser. On-disk layout in <shirudo/upatch.h>.
 */

#ifndef SHIRUDO_UTIL_UPATCH_H
#define SHIRUDO_UTIL_UPATCH_H

#include <shirudo/misc.h>
#include <shirudo/upatch.h>
#include <dirent.h>
#include <stdbool.h>
#include <stddef.h>

/* The largest container the parser accepts. */
#define SHIRUDO_UPATCH_FILE_MAX		(16u * 1024u * 1024u)

struct shirudo_upatch {
	struct shirudo_upatch_header	hdr;	/* host byte order */
	const __u8			*map;
	size_t				map_size;
	const struct shirudo_upatch_decoder_blob *decoder_blob;
	__u32				decoder_blob_len;
	__u32				nr_decoders;
};

struct shirudo_upatch_decoder_view {
	__u32		language;
	__u32		api_version;
	__u32		input_format;
	__u32		export_signal;
	char		upatch[64];
	char		map[16];
	char		source_name[64];
	char		entrypoint[32];
	const char	*source;
	__u32		source_len;
};

/*
 * Returns 0 on success, -errno on failure. Logs nothing -- caller
 * decides whether to emit user-facing diagnostics.
 */
int  shirudo_upatch_open(const char *path, struct shirudo_upatch *out);
void shirudo_upatch_close(struct shirudo_upatch *u);

/*
 * Section accessors. Pointers reference the mmap; valid only until
 * shirudo_upatch_close().
 */
const void *shirudo_upatch_loader(const struct shirudo_upatch *u);
const void *shirudo_upatch_signature(const struct shirudo_upatch *u);
const void *shirudo_upatch_metadata(const struct shirudo_upatch *u);
const void *shirudo_upatch_data(const struct shirudo_upatch *u);

bool shirudo_upatch_is_unload(const struct shirudo_upatch *u);
const char *shirudo_upatch_type_str(const struct shirudo_upatch *u);

bool shirudo_upatch_has_decoders(const struct shirudo_upatch *u);
int shirudo_upatch_decoder_view(const struct shirudo_upatch *u, __u32 idx,
				struct shirudo_upatch_decoder_view *out);

/*
 * Validate an externally-provided upatch name: non-empty, doesn't
 * start with '.', and matches [A-Za-z0-9._-]+. Rejects anything that
 * could escape the bpffs / on-disk path namespace.
 */
bool shirudo_upatch_name_valid(const char *name);

/* A scandir() filter for the archived .upatch files. */
int shirudo_upatch_select(const struct dirent *e);

#endif /* SHIRUDO_UTIL_UPATCH_H */
