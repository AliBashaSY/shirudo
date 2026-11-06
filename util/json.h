/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * In-tree JSON emitter and schema-callback parser.
 */

#ifndef SHIRUDO_JSON_H
#define SHIRUDO_JSON_H

#include <stdbool.h>
#include <stddef.h>

#include <shirudo/misc.h>

#ifndef JSON_MAX_BYTES
#define JSON_MAX_BYTES		(8u * 1024u * 1024u)
#endif
#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH		32u
#endif
#ifndef JSON_MAX_STRING_BYTES
#define JSON_MAX_STRING_BYTES	(16u * 1024u * 1024u)
#endif

struct json_emit {
	char		*buf;
	size_t		cap;
	size_t		len;
	int		err;	/* sticky; -ENOSPC, -EILSEQ, -E2BIG */
	__u32		depth;	/* open-container nesting */
	__u32		nonempty; /* bit d set: container at depth d has a member */
	bool		value_pending;
};

/*
 * Commas and escaping are automatic. On any error the sticky err
 * latches and later calls are no-ops: check it once after the final
 * close.
 */
void json_obj_open(struct json_emit *e);
void json_obj_open_key(struct json_emit *e, const char *key);
/* Value containers and scalars consume a pending json_key or array slot. */
void json_obj_value_open(struct json_emit *e);
void json_obj_close(struct json_emit *e);
void json_arr_open(struct json_emit *e, const char *key);
void json_arr_open_value(struct json_emit *e);
void json_arr_close(struct json_emit *e);
void json_key(struct json_emit *e, const char *key);
void json_kv_string(struct json_emit *e,
		    const char *key, const char *val);
void json_kv_int(struct json_emit *e,
		 const char *key, __s64 val);
void json_kv_bool(struct json_emit *e,
		  const char *key, int val);
void json_kv_null(struct json_emit *e, const char *key);
/* Insert an internally-generated, already-valid JSON value verbatim. */
void json_kv_raw(struct json_emit *e, const char *key,
		 const char *value, size_t value_len);
void json_str(struct json_emit *e, const char *val);
void json_str_value(struct json_emit *e, const char *val);
void json_raw_value(struct json_emit *e, const char *value,
		    size_t value_len);
void json_raw(struct json_emit *e, const char *value,
	      size_t value_len);

/*
 * Not a generic deserialiser: declared keys are dispatched, everything
 * else is parsed and discarded, within bounds so hostile input cannot
 * exhaust the stack or a buffer.
 */
enum json_vtype {
	JSON_T_STRING,	/* dst: char[dst_cap] (NUL-terminated)   */
	JSON_T_INT,	/* dst: __s64 *                          */
	JSON_T_BOOL,	/* dst: int *  (0/1)                     */
	JSON_T_OBJECT,	/* nested object -> sub[] sub-schema     */
	JSON_T_ARRAY,	/* sub[] schema; dst = shirudo_json_array * */
};

/*
 * The seen flags of the sub fields, and of the fields nested in them,
 * reset per element; the dst buffers do not. on_elem may copy the
 * element out of the scratch dst buffers before the next one overwrites
 * them, and returns a negative errno to abort.
 */
struct json_array {
	int	(*on_elem)(void *ctx);
	void	*ctx;
};

struct json_field {
	const char			*key;
	enum json_vtype		type;
	void				*dst;
	size_t				dst_cap;	/* STRING only */
	int				seen;		/* set by the parser */
	struct json_field	*sub;		/* OBJECT/ARRAY schema */
	size_t				n_sub;		/* OBJECT/ARRAY schema */
};

/*
 * -EINVAL on malformed input or a type mismatch, -E2BIG on a bounds
 * violation, -ENOSPC when a string exceeds its dst_cap.
 */
int json_parse(const char *data, size_t len,
	       struct json_field *fields, size_t n_fields);

#endif /* SHIRUDO_JSON_H */
