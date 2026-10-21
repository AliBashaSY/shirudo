// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Drives the measurement register against a TPM (SHIRUDO_TPM_TCTI) with
 * the eventlog and certificate redirected by environment. Subcommand on
 * argv[1]; prints what a shell test asserts on.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <shirudo/misc.h>

#include "attest.h"
#include "chain.h"
#include "config.h"
#include "eventlog.h"
#include "hex.h"
#include "json.h"
#include "malloc.h"
#include "paths.h"
#include "tpm.h"

static void puthex(const __u8 *b, size_t n)
{
	char h[2 * 1024 + 1];

	if (n > 1024)
		return;
	hex_encode(b, n, h);
	printf("%s\n", h);
}

/* Record an init the way shirudo init does, minus the set details. */
static int append_init(const __u8 event[32],
		       const struct shirudo_chain_init_info *info)
{
	char buf[4096], ts[32], boot_id[64], nv_index[16], hex[256];
	struct shirudo_json_emit e = { .buf = buf, .cap = sizeof(buf) };

	eventlog_timestamp(ts, sizeof(ts));
	eventlog_boot_id(boot_id, sizeof(boot_id));
	snprintf(nv_index, sizeof(nv_index), "0x%08X", SHIRUDO_NV_INDEX);
	json_obj_open(&e);
	json_kv_int(&e, "schema", 2);
	json_kv_string(&e, "ts", ts);
	json_kv_string(&e, "nv_index", nv_index);
	json_kv_string(&e, "alg", "sha256");
	hex_encode(event, 32, hex);
	json_kv_string(&e, "digest", hex);
	json_kv_string(&e, "type", "init");
	json_kv_string(&e, "boot_id", boot_id);
	json_kv_int(&e, "tpm_reset_count", info->reset_count);
	json_kv_int(&e, "pcr", SHIRUDO_CHAIN_PCR);
	hex_encode(info->pcr_before, 32, hex);
	json_kv_string(&e, "pcr_before", hex);
	json_kv_int(&e, "phase_pcr", SHIRUDO_CHAIN_PHASE_PCR);
	hex_encode(info->phase_before, 32, hex);
	json_kv_string(&e, "phase_before", hex);
	json_kv_string(&e, "bound_in", info->initrd ? "initrd" : "root");
	hex_encode(info->name, info->name_len, hex);
	json_kv_string(&e, "nv_name", hex);
	json_kv_string(&e, "anchor", info->anchor);
	hex_encode(info->anchor_digest, 32, hex);
	json_kv_string(&e, "anchor_digest", hex);
	hex_encode(info->value, 32, hex);
	json_kv_string(&e, "chain", hex);
	json_obj_close(&e);
	if (e.err)
		return e.err;
	return eventlog_append(buf);
}

static int append_event(const char *type, const char *name,
			const __u8 event[32], const __u8 chain[32])
{
	char buf[4096], ts[32], boot_id[64], nv_index[16], hex[65];
	struct shirudo_json_emit e = { .buf = buf, .cap = sizeof(buf) };

	eventlog_timestamp(ts, sizeof(ts));
	eventlog_boot_id(boot_id, sizeof(boot_id));
	snprintf(nv_index, sizeof(nv_index), "0x%08X", SHIRUDO_NV_INDEX);
	json_obj_open(&e);
	json_kv_int(&e, "schema", 2);
	json_kv_string(&e, "ts", ts);
	json_kv_string(&e, "nv_index", nv_index);
	json_kv_string(&e, "alg", "sha256");
	hex_encode(event, 32, hex);
	json_kv_string(&e, "digest", hex);
	json_kv_string(&e, "type", type);
	json_kv_string(&e, "boot_id", boot_id);
	json_obj_open_key(&e, "upatch");
	json_kv_string(&e, "name", name);
	json_obj_close(&e);
	hex_encode(chain, 32, hex);
	json_kv_string(&e, "chain", hex);
	json_obj_close(&e);
	if (e.err)
		return e.err;
	return eventlog_append(buf);
}

int main(int argc, char **argv)
{
	__u8 event[32], chain[32];
	int rc;

	if (argc < 2)
		return 2;

	if (strcmp(argv[1], "init") == 0 && argc == 3) {
		struct shirudo_chain_init_info info;

		if (hex_decode(argv[2], event, 32))
			return 2;
		rc = chain_init(event, &info);
		printf("rc=%d\n", rc);
		if (rc)
			return 1;
		printf("reused=%d\n", info.reused);
		printf("reset_count=%u\n", info.reset_count);
		printf("value=");
		puthex(info.value, 32);
		printf("name=");
		puthex(info.name, info.name_len);
		printf("pcr_before=");
		puthex(info.pcr_before, 32);
		printf("phase_before=");
		puthex(info.phase_before, 32);
		printf("bound_in=%s\n", info.initrd ? "initrd" : "root");
		printf("anchor=%s\n", info.anchor);
		if (!info.reused && append_init(event, &info))
			return 1;
		return 0;
	}
	if (strcmp(argv[1], "load") == 0 && argc == 4) {
		__u8 payload[32];

		if (hex_decode(argv[3], payload, 32))
			return 2;
		chain_upatch_event("shirudo-upatch", argv[2], payload,
				   event);
		rc = chain_extend(event, chain);
		printf("rc=%d\n", rc);
		if (rc)
			return 1;
		printf("value=");
		puthex(chain, 32);
		return append_event("upatch_load", argv[2], event, chain) ?
		       1 : 0;
	}
	if (strcmp(argv[1], "extend-raw") == 0 && argc == 3) {
		if (hex_decode(argv[2], event, 32))
			return 2;
		rc = chain_extend(event, chain);
		printf("rc=%d\n", rc);
		if (rc)
			return 1;
		printf("value=");
		puthex(chain, 32);
		return 0;
	}
	if (strcmp(argv[1], "read") == 0) {
		rc = chain_read(chain);
		printf("rc=%d\n", rc);
		if (rc)
			return 1;
		printf("value=");
		puthex(chain, 32);
		return 0;
	}
	if (strcmp(argv[1], "status") == 0) {
		struct chain_status st;

		rc = chain_status(&st);
		printf("rc=%d\n", rc);
		printf("tpm=%d defined=%d written=%d logged=%d anchored=%d\n",
		       st.tpm, st.defined, st.written, st.logged, st.anchored);
		printf("rebound=%d\n", st.rebound);
		if (st.defined && st.written) {
			printf("value=");
			puthex(st.value, 32);
		}
		return rc ? 1 : 0;
	}
	/* A measurement into a PCR, as systemd's boot phases make them. */
	if (strcmp(argv[1], "pcr-extend") == 0 && argc == 4) {
		struct shirudo_tpm *tpm = NULL;
		__u8 d[32];

		if (hex_decode(argv[3], d, 32) || tpm_open(&tpm))
			return 2;
		rc = tpm_pcr_extend(tpm, (__u32)atoi(argv[2]), d);
		tpm_close(tpm);
		printf("rc=%d\n", rc);
		return rc ? 1 : 0;
	}
	if (strcmp(argv[1], "pcr") == 0 && argc == 3) {
		struct shirudo_tpm *tpm = NULL;
		__u8 v[32];

		if (tpm_open(&tpm))
			return 1;
		rc = tpm_pcr_read(tpm, (__u32)atoi(argv[2]), v);
		tpm_close(tpm);
		if (rc)
			return 1;
		puthex(v, 32);
		return 0;
	}
	/*
	 * The endorsement key of a type, as a target without a certificate
	 * presents it.
	 */
	if (strcmp(argv[1], "ek-public") == 0 && argc == 3) {
		struct shirudo_tpm_key ek = { .handle = 0 };
		struct shirudo_tpm *tpm = NULL;

		if (tpm_open(&tpm))
			return 1;
		rc = tpm_ek_create(tpm, strcmp(argv[2], "rsa") == 0,
				   &ek);
		if (!rc) {
			printf("public=");
			puthex(ek.public, ek.public_len);
			printf("name=");
			puthex(ek.name, ek.name_len);
			tpm_key_flush(tpm, &ek);
		}
		tpm_close(tpm);
		printf("rc=%d\n", rc);
		return rc ? 1 : 0;
	}
	/* The attack: recreate the index with the same public area. */
	if (strcmp(argv[1], "recreate") == 0) {
		struct tpm_nv_info nv;
		struct shirudo_tpm *tpm = NULL;

		if (tpm_open(&tpm))
			return 1;
		rc = tpm_nv_info(tpm, SHIRUDO_NV_INDEX, &nv);
		if (!rc && nv.defined)
			rc = tpm_nv_undefine(tpm, SHIRUDO_NV_INDEX,
					     NULL, 0);
		if (!rc)
			rc = tpm_nv_define(tpm, SHIRUDO_NV_INDEX,
					   nv.policy, NULL, 0);
		if (!rc)
			rc = tpm_nv_info(tpm, SHIRUDO_NV_INDEX, &nv);
		tpm_close(tpm);
		printf("rc=%d\n", rc);
		if (rc)
			return 1;
		printf("written=%d\nname=", nv.written);
		puthex(nv.name, nv.name_len);
		return 0;
	}
	/* The attack: try the init branch again after the anchor. */
	if (strcmp(argv[1], "first-write") == 0 && argc == 3) {
		__u8 branch_written[32], branch_init[32], policy[32];
		struct shirudo_eventlog_init rec;
		struct shirudo_tpm *tpm = NULL;
		__u8 values[64];

		if (hex_decode(argv[2], event, 32) ||
		    eventlog_boot_init(&rec))
			return 2;
		memcpy(values, rec.pcr_before, 32);
		memcpy(values + 32, rec.phase_before, 32);
		tpm_register_policy(SHIRUDO_CHAIN_PCR_MASK, values, 2,
				    branch_written, branch_init,
				    policy);
		if (tpm_open(&tpm))
			return 1;
		rc = tpm_nv_extend_first(tpm, SHIRUDO_NV_INDEX,
					 SHIRUDO_CHAIN_PCR_MASK,
					 branch_written, branch_init,
					 event);
		tpm_close(tpm);
		printf("rc=%d\n", rc);
		return rc ? 1 : 0;
	}
	if (strcmp(argv[1], "evidence") == 0 && (argc == 3 || argc == 4)) {
		struct shirudo_attest_challenge challenge = {};
		__u8 nonce[64];
		size_t nonce_len = strlen(argv[2]) / 2, len, cap;
		char *doc;

		if (nonce_len > sizeof(nonce) ||
		    hex_decode(argv[2], nonce, nonce_len))
			return 2;
		if (argc == 4 &&
		    attest_challenge_read(argv[3], &challenge)) {
			fprintf(stderr, "evidence: bad challenge\n");
			return 2;
		}
		rc = attest_evidence(nonce, nonce_len,
				     SHIRUDO_ATTEST_PCR_DEFAULT,
				     argc == 4 ? &challenge : NULL,
				     &doc, &len, &cap);
		if (rc) {
			fprintf(stderr, "evidence: %s\n", strerror(-rc));
			return 1;
		}
		fwrite(doc, 1, len, stdout);
		fputc('\n', stdout);
		xfree(doc, cap);
		return 0;
	}
	return 2;
}
