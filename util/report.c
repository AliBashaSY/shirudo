// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Per-operation JSON report assembly (upatch load/unload --report).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include <shirudo/string.h>

#include "json.h"
#include "log.h"
#include "malloc.h"
#include "paths.h"
#include "report.h"

int report_emit(const char *path, const struct shirudo_op_report *r)
{
	struct shirudo_json_emit e;
	struct utsname uts;
	char nv_index[16];
	const char *errname;
	const char *vlog = r->verifier_log ? r->verifier_log : "";
	size_t loglen = strlen(vlog);
	size_t cap = loglen * 6 + 16384;	/* worst-case escape + slack */
	char *buf;
	FILE *f;
	int rc = 0;

	if (strsprintf(nv_index, sizeof(nv_index), "0x%08X",
		       SHIRUDO_NV_INDEX) < 0)
		return -E2BIG;
	buf = xmalloc(cap);
	if (!buf)
		return -ENOMEM;
	e = (struct shirudo_json_emit){ .buf = buf, .cap = cap };

	errname = r->err ? strerrorname_np(r->err) : NULL;

	json_obj_open(&e);
	json_kv_int(&e, "schema", 1);
	json_kv_string(&e, "result", r->ok ? "ok" : "failed");
	json_kv_string(&e, "op", r->op ? r->op : "");
	json_kv_string(&e, "phase", r->phase ? r->phase : "done");
	if (r->err)
		json_kv_int(&e, "errno", r->err);
	else
		json_kv_null(&e, "errno");
	if (errname)
		json_kv_string(&e, "errno_name", errname);
	else
		json_kv_null(&e, "errno_name");
	json_kv_string(&e, "category", r->category ? r->category
						: (r->ok ? "ok" : "unknown"));
	json_kv_string(&e, "message", r->message);
	json_kv_string(&e, "verifier_log", vlog);
	json_kv_int(&e, "loader_return", r->loader_return);

	json_obj_open_key(&e, "upatch");
	json_kv_string(&e, "path", r->path ? r->path : "");
	json_kv_string(&e, "name", r->name ? r->name : "");
	json_kv_string(&e, "payload_sha256", r->payload_hex);
	json_obj_close(&e);

	json_obj_open_key(&e, "kernel");
	json_kv_string(&e, "release",
		       uname(&uts) == 0 ? uts.release : "");
	json_obj_close(&e);

	if (r->ok && r->tpm_present) {
		json_obj_open_key(&e, "tpm");
		json_kv_string(&e, "nv_index", nv_index);
		json_kv_string(&e, "chain_after", r->chain_after);
		json_obj_close(&e);
	} else {
		json_kv_null(&e, "tpm");
	}
	json_obj_close(&e);

	if (e.err) {
		pr_err("report: encode failed: %s", strerror(-e.err));
		xfree(buf, cap);
		return e.err;
	}

	if (strcmp(path, "-") == 0) {
		printf("%s\n", buf);
		xfree(buf, cap);
		return 0;
	}
	f = fopen(path, "w");
	if (!f) {
		rc = -errno;
		pr_err("report: open %s: %s", path, strerror(errno));
		xfree(buf, cap);
		return rc;
	}
	rc = fprintf(f, "%s\n", buf) < 0 ? -EIO : 0;
	if (fclose(f) != 0 && !rc)
		rc = -errno;
	if (rc)
		pr_err("report: write %s: %s", path, strerror(-rc));
	xfree(buf, cap);
	return rc;
}

void report_set_log(struct shirudo_op_report *r, const char *log)
{
	if (!r)
		return;
	xstrfree(r->verifier_log);
	r->verifier_log = NULL;
	if (!log || !*log)
		return;
	/*
	 * The verifier log is kernel-provided diagnostic output -- bound
	 * the read, copy with xstrndup, and scrub on free via xstrfree.
	 */
	r->verifier_log = xstrndup(log, SHIRUDO_VERIFIER_LOG_MAX);
}

void report_free(struct shirudo_op_report *r)
{
	xstrfree(r->verifier_log);
	r->verifier_log = NULL;
}
