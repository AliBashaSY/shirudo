/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * In-tree JSON emitter and schema-callback parser.
 */

#ifndef SHIRUDO_JSON_H
#define SHIRUDO_JSON_H

#include <stddef.h>
#include <stdint.h>

#ifndef SHIRUDO_JSON_MAX_BYTES
#define SHIRUDO_JSON_MAX_BYTES		(8u * 1024u * 1024u)
#endif
#ifndef SHIRUDO_JSON_MAX_DEPTH
#define SHIRUDO_JSON_MAX_DEPTH		32u
#endif
#ifndef SHIRUDO_JSON_MAX_STRING_BYTES
#define SHIRUDO_JSON_MAX_STRING_BYTES	(16u * 1024u * 1024u)
#endif

struct shirudo_json_emit {
	char		*buf;
	size_t		cap;
	size_t		len;
	int		err;	/* sticky; -ENOSPC, -EILSEQ, -E2BIG */
	unsigned	depth;	/* open-container nesting */
	uint32_t	nonempty; /* bit d set: container at depth d has a member */
};

/*
 * Commas and escaping are automatic. On any error the sticky err
 * latches and later calls are no-ops: check it once after the final
 * close.
 */
void shirudo_json_obj_open(struct shirudo_json_emit *e);
void shirudo_json_obj_open_key(struct shirudo_json_emit *e, const char *key);
void shirudo_json_obj_close(struct shirudo_json_emit *e);
void shirudo_json_arr_open(struct shirudo_json_emit *e, const char *key);
void shirudo_json_arr_close(struct shirudo_json_emit *e);
void shirudo_json_kv_string(struct shirudo_json_emit *e,
			    const char *key, const char *val);
void shirudo_json_kv_int(struct shirudo_json_emit *e,
			 const char *key, int64_t val);
void shirudo_json_kv_bool(struct shirudo_json_emit *e,
			  const char *key, int val);
void shirudo_json_kv_null(struct shirudo_json_emit *e, const char *key);
/* Insert an internally-generated, already-valid JSON value verbatim. */
void shirudo_json_kv_raw(struct shirudo_json_emit *e, const char *key,
			 const char *value, size_t value_len);
void shirudo_json_str(struct shirudo_json_emit *e, const char *val);
void shirudo_json_raw(struct shirudo_json_emit *e, const char *value,
		      size_t value_len);

/*
 * Not a generic deserialiser: declared keys are dispatched, everything
 * else is parsed and discarded, within bounds so hostile input cannot
 * exhaust the stack or a buffer.
 */
enum shirudo_json_vtype {
	SHIRUDO_JSON_T_STRING,	/* dst: char[dst_cap] (NUL-terminated)   */
	SHIRUDO_JSON_T_INT,	/* dst: int64_t *                        */
	SHIRUDO_JSON_T_BOOL,	/* dst: int *  (0/1)                     */
	SHIRUDO_JSON_T_OBJECT,	/* nested object -> sub[] sub-schema     */
	SHIRUDO_JSON_T_ARRAY,	/* sub[] schema; dst = shirudo_json_array * */
};

/*
 * The sub fields' seen flags reset per element; on_elem may copy the
 * element out of the scratch dst buffers before the next one overwrites
 * them, and returns a negative errno to abort.
 */
struct shirudo_json_array {
	int	(*on_elem)(void *ctx);
	void	*ctx;
};

struct shirudo_json_field {
	const char			*key;
	enum shirudo_json_vtype		type;
	void				*dst;
	size_t				dst_cap;	/* STRING only */
	int				seen;		/* set by the parser */
	struct shirudo_json_field	*sub;		/* OBJECT/ARRAY schema */
	size_t				n_sub;		/* OBJECT/ARRAY schema */
};

/*
 * -EINVAL on malformed input or a type mismatch, -E2BIG on a bounds
 * violation, -ENOSPC when a string exceeds its dst_cap.
 */
int shirudo_json_parse(const char *data, size_t len,
		       struct shirudo_json_field *fields, size_t n_fields);

#endif /* SHIRUDO_JSON_H */
