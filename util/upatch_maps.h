/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The pinned maps of loaded upatches, for `upatch map` and status.
 * Arena maps are read through the upatch's serializer alone.
 */

#ifndef SHIRUDO_UTIL_UPATCH_MAPS_H
#define SHIRUDO_UTIL_UPATCH_MAPS_H

#include <stdbool.h>
#include <stddef.h>

#include <shirudo/misc.h>
#include <shirudo/upatch.h>

#include "upatch_resolve.h"

/* Default truncation cap; --show-full lifts it. */
#define SHIRUDO_MAP_ENTRIES_DEFAULT	256

#define SHIRUDO_MAP_NAME_MAX		16	/* BPF_OBJ_NAME_LEN */
#define SHIRUDO_MAP_TYPENAME_MAX	64

struct shirudo_json_emit;

struct shirudo_map_entry {
	void		*key;		/* key_size bytes */
	void		*value;		/* value_stride * (per-CPU ? n_cpus : 1) */
};

/*
 * SHIRUDO_MAP_TYPEDEF_MAX bounds the multi-line struct/array
 * "definition" string captured at read time (struct foo { __u32 a; ... }).
 * Big enough for the common shirudo case (~10 fields); structs larger
 * than this are truncated with "; ..." and the renderer still works.
 */
#define SHIRUDO_MAP_TYPEDEF_MAX		1024

struct btf;

struct shirudo_serial_page {
	__u32		page_index;
	__u64		cursor;
	__u64		next_cursor;
	__u32		flags;
	__u32		chunks;
	__u32		bytes;
	bool		done;
	void		*data;
};

struct shirudo_map_view {
	char		upatch[SHIRUDO_UPATCH_NAME_MAX + 1];
	char		name[SHIRUDO_MAP_NAME_MAX + 1];
	char		location[SHIRUDO_UPATCH_LOCATION_MAX];
	__u32		type;		/* BPF_MAP_TYPE_* */
	const char	*type_name;	/* "percpu_array" / "hash" / ... */
	__u32		key_size;
	__u32		value_size;	/* per-CPU value size (not multiplied) */
	__u32		value_stride;	/* per-CPU kernel ABI stride; else value_size */
	__u32		max_entries;
	__u32		n_entries;	/* live entries iterated */
	__u32		n_cpus;		/* 1 for non-percpu types */
	__u64		lifetime_start_ns; /* pin-dir ctime; metric reset boundary */
	bool		truncated;
	__u32		n_truncated_at;
	bool		serialized;
	__u32		n_serial_pages;
	__u64		serial_bytes_total;
	struct shirudo_serial_page *serial_pages;
	bool		skipped;
	const char	*skipped_reason;
	char		btf_key_type[SHIRUDO_MAP_TYPENAME_MAX];
	char		btf_value_type[SHIRUDO_MAP_TYPENAME_MAX];
	char		key_type_def[SHIRUDO_MAP_TYPEDEF_MAX];
	char		value_type_def[SHIRUDO_MAP_TYPEDEF_MAX];
	__u32		key_type_id;
	__u32		value_type_id;
	struct btf	*btf;		/* heap; owned by the view */
	struct shirudo_map_entry	*entries;	/* n_entries long */
};

void map_view_free(struct shirudo_map_view *v);

/* Name, type and max_entries alone, without reading entries. */
struct shirudo_map_summary {
	char		upatch[SHIRUDO_UPATCH_NAME_MAX + 1];
	char		name[SHIRUDO_MAP_NAME_MAX + 1];
	__u32		type;
	const char	*type_name;
	__u32		max_entries;
	__u32		n_cpus;
};

int  upatch_maps_list_all(struct shirudo_map_summary **out,
			  size_t *n_out);
int  upatch_maps_list_one(const char *upatch,
			  struct shirudo_map_summary **out,
			  size_t *n_out);
void map_summary_free(struct shirudo_map_summary *list, size_t n);

/*
 * Read one map's contents. <max_entries_cap> bounds the number of
 * (key, value) pairs returned; SHIRUDO_MAP_ENTRIES_DEFAULT for the
 * default, 0 for unlimited (--show-full). Returns 0/-errno.
 */
int  upatch_maps_read(const char *upatch, const char *map,
		      __u32 max_entries_cap,
		      struct shirudo_map_view *out);

/*
 * Serializer-backed maps are drained by a read, as the map-export timer
 * expects. SHIRUDO_MAP_READ_PEEK asks the serializer for the interval
 * being collected without taking it from the exporter.
 */
#define SHIRUDO_MAP_READ_PEEK	(1u << 0)

int  upatch_maps_read_flags(const char *upatch, const char *map,
			    __u32 max_entries_cap, __u32 flags,
			    struct shirudo_map_view *out);

void upatch_maps_print_summary(const struct shirudo_map_summary *list,
			       size_t n);
void upatch_maps_print_detail(const struct shirudo_map_view *v);

/* NULL v emits nothing. The shape is part of the status report. */
void upatch_maps_emit_json_fields(struct shirudo_json_emit *e,
				  const struct shirudo_map_view *v);
void upatch_maps_emit_json_object(struct shirudo_json_emit *e,
				  const struct shirudo_map_view *v);

bool upatch_map_is_percpu(__u32 type);
__u32 upatch_map_value_stride(const struct shirudo_map_view *v);
char *upatch_map_format_cell(const struct shirudo_map_view *v,
			     __u32 type_id, const void *value,
			     __u32 value_size,
			     const char *btf_type);
char *upatch_map_format_percpu_sum(const struct shirudo_map_view *v,
				   const void *values_per_cpu,
				   const char *btf_type);

int prog_test_run(int prog_fd, void *ctx, __u32 ctx_sz,
		  __u32 *retval);

#endif /* SHIRUDO_UTIL_UPATCH_MAPS_H */
