// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Unified upatches-loaded table; see util/upatch_table.h.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <shirudo/misc.h>

#include "json.h"
#include "timefmt.h"
#include "upatch_table.h"

__s64 upatch_age_sec(const char *ts)
{
	struct tm tm = {};

	if (!ts || !ts[0])
		return -1;
	if (!strptime(ts, "%Y-%m-%dT%H:%M:%SZ", &tm))
		return -1;
	return (__s64)(time(NULL) - timegm(&tm));
}

void upatch_table_print(const struct upatch_table_row *rows,
			size_t n)
{
	size_t i;

	printf("\nupatches loaded (%zu):\n", n);
	if (n == 0)
		return;
	printf("  %-24s  %-28s  %5s  %-28s  %s\n",
	       "NAME", "LOCATION", "MAPS", "STATUS", "AGE");
	for (i = 0; i < n; i++) {
		const struct upatch_table_row *r = &rows[i];
		char age[16] = "?";

		if (r->age_sec >= 0)
			format_duration((long)r->age_sec,
					age, sizeof(age));
		printf("  %-24s  %-28s  %5u  %-28s  %s\n", r->name,
		       r->location[0] ? r->location : "-",
		       r->n_maps, r->status[0] ? r->status : "success", age);
	}
}

void upatch_table_emit_json(struct json_emit *e,
			    const struct upatch_table_row *rows,
			    size_t n)
{
	size_t i;

	json_arr_open(e, "upatches_loaded");
	for (i = 0; i < n; i++) {
		const struct upatch_table_row *r = &rows[i];

		json_obj_open(e);
		json_kv_string(e, "name", r->name);
		json_kv_string(e, "location", r->location);
		json_kv_string(e, "status",
			       r->status[0] ? r->status : "success");
		json_kv_int(e, "n_maps", (__s64)r->n_maps);
		json_kv_string(e, "loaded_at", r->loaded_at);
		if (r->age_sec >= 0)
			json_kv_int(e, "age_sec", r->age_sec);
		else
			json_kv_null(e, "age_sec");
		if (r->payload_sha256[0])
			json_kv_string(e, "payload_sha256",
				       r->payload_sha256);
		else
			json_kv_null(e, "payload_sha256");
		json_obj_close(e);
	}
	json_arr_close(e);
}
