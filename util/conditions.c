// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Shared local security conditions block + status verdict.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <shirudo/misc.h>
#include <shirudo/string.h>

#include "conditions.h"

static bool config_untrusted(const struct shirudo_conditions_input *in)
{
	return in->config_seen && in->config_state &&
	       in->config_state != -EINPROGRESS;
}

static const char *config_untrusted_detail(int state)
{
	if (state == -ESTALE)
		return "the config activation record does not belong to this TPM boot (tamper?)";
	if (state == -EKEYREJECTED)
		return "the mounted config does not match the selected root (tamper?)";
	return "the config PCR, activation records and mounted config disagree (tamper?)";
}

size_t conditions(const struct shirudo_conditions_input *in,
		  struct shirudo_condition *c, size_t cap)
{
	size_t n = 0;

	if (n < cap) {
		c[n].type = "LsmAttached";
		c[n].status = in->lsm_attached ? "OK" : "ALERT";
		strscpy(c[n].detail, in->lsm_attached ?
			"lsm/bpf link present and pinned" :
			"lsm/bpf absent -- enforcement OFF",
			sizeof(c[n].detail));
		n++;
	}
	if (n < cap) {
		c[n].type = "KeyringSealed";
		if (!in->keyring_present && in->ksp_seen) {
			c[n].status = "ALERT";
			if (strsprintf(c[n].detail, sizeof(c[n].detail),
				       "pin serial=%lld, the BPF keyring is "
				       "gone (removed?)",
				       (__s64)in->keyring_serial_pinned) < 0)
				strscpy(c[n].detail, "?", sizeof(c[n].detail));
		} else if (!in->keyring_present) {
			c[n].status = "WARN";
			strscpy(c[n].detail,
				"shirudo keyring absent (not init'd?)",
				sizeof(c[n].detail));
		} else if (in->ksm_seen && !in->keyring_serial_matches) {
			c[n].status = "ALERT";
			if (strsprintf(c[n].detail, sizeof(c[n].detail),
				       "pin serial=%lld != live serial=%lld "
				       "(swapped?)",
				       (__s64)in->keyring_serial_pinned,
				       (__s64)in->keyring_serial) < 0)
				strscpy(c[n].detail, "?", sizeof(c[n].detail));
		} else {
			c[n].status = "OK";
			if (strsprintf(c[n].detail, sizeof(c[n].detail),
				       "serial=%lld in the BPF keyring, %s",
				       (__s64)in->keyring_serial,
				       in->ksp_seen ? "pin matches" :
				       "no pin recorded") < 0)
				strscpy(c[n].detail, "?", sizeof(c[n].detail));
		}
		n++;
	}
	if (n < cap) {
		c[n].type = "ChainIntact";
		if (!in->chain_seen) {
			c[n].status = "WARN";
			strscpy(c[n].detail, in->tpm_reachable ?
				"no register bound this boot (init incomplete)" :
				"no TPM answered, no register to replay",
				sizeof(c[n].detail));
		} else if (in->cm_seen && in->chain_matches) {
			c[n].status = "OK";
			strscpy(c[n].detail, "live register == eventlog replay",
				sizeof(c[n].detail));
		} else if (in->cm_seen) {
			c[n].status = "ALERT";
			strscpy(c[n].detail,
				"live register != eventlog replay (tamper?)",
				sizeof(c[n].detail));
		} else {
			c[n].status = "WARN";
			strscpy(c[n].detail,
				"chain present, replay not evaluated",
				sizeof(c[n].detail));
		}
		n++;
	}
	if (n < cap) {
		c[n].type = "TpmMeasured";
		c[n].status = in->chain_seen ? "OK" : "WARN";
		if (in->chain_seen) {
			if (strsprintf(c[n].detail, sizeof(c[n].detail),
				       "NV index %s readable",
				       in->nv_index ? in->nv_index : "?") < 0)
				strscpy(c[n].detail, "?", sizeof(c[n].detail));
		} else if (in->tpm_reachable) {
			if (strsprintf(c[n].detail, sizeof(c[n].detail),
				       "NV index %s not bound this boot",
				       in->nv_index ? in->nv_index : "?") < 0)
				strscpy(c[n].detail, "?", sizeof(c[n].detail));
		} else {
			strscpy(c[n].detail, "no TPM answered",
				sizeof(c[n].detail));
		}
		n++;
	}
	if (n < cap) {
		const char *root = in->config_root ? in->config_root : "?";

		c[n].type = "ConfigActive";
		c[n].status = "OK";
		if (!in->config_seen) {
			c[n].status = "WARN";
			strscpy(c[n].detail, "no config activation state (init not run?)",
				sizeof(c[n].detail));
		} else if (in->config_state == -EINPROGRESS) {
			c[n].status = "WARN";
			if (strsprintf(c[n].detail, sizeof(c[n].detail),
				       "the activation of config %.12s.. is pending until the next reboot",
				       root) < 0)
				strscpy(c[n].detail, "?", sizeof(c[n].detail));
		} else if (config_untrusted(in)) {
			c[n].status = "ALERT";
			strscpy(c[n].detail, config_untrusted_detail(in->config_state),
				sizeof(c[n].detail));
		} else if (strsprintf(c[n].detail, sizeof(c[n].detail),
				      "config %.12s.. is active", root) < 0) {
			strscpy(c[n].detail, "?", sizeof(c[n].detail));
		}
		n++;
	}
	return n;
}

void conditions_print(const struct shirudo_condition *c, size_t n)
{
	size_t i;

	printf("Conditions:\n");
	printf("  %-16s %-7s %s\n", "TYPE", "STATUS", "DETAIL");
	for (i = 0; i < n; i++)
		printf("  %-16s %-7s %s\n", c[i].type, c[i].status,
		       c[i].detail);
}

const char *status_verdict(const struct shirudo_conditions_input *in)
{
	if (!in->reachable)
		return "unreachable";
	if (!in->parsed)
		return "unparseable";
	if ((in->cm_seen && !in->chain_matches) ||
	    (in->ksm_seen && !in->keyring_serial_matches) ||
	    (in->ksp_seen && !in->keyring_present) || config_untrusted(in))
		return "tampered";
	if (!in->lsm_attached)
		return "lsm-off";
	if (!in->keyring_present)
		return "init-pending";
	if (in->config_seen && in->config_state == -EINPROGRESS)
		return "reboot-pending";
	if (!in->chain_seen)
		return "degraded";
	return "ok";
}

void status_reason(const struct shirudo_conditions_input *in,
		   char *buf, size_t cap)
{
	const char *v = status_verdict(in);

	buf[0] = '\0';
	if (strcmp(v, "unreachable") == 0) {
		if (in->unreach)
			strscpy(buf, in->unreach, cap);
	} else if (strcmp(v, "unparseable") == 0) {
		strscpy(buf, "status JSON unparseable", cap);
	} else if (strcmp(v, "tampered") == 0) {
		if (in->cm_seen && !in->chain_matches)
			strscpy(buf, "live register != eventlog replay", cap);
		else if (config_untrusted(in))
			strscpy(buf, "the selected config cannot be verified against its records",
				cap);
		else if (!in->keyring_present)
			strscpy(buf, "the keyring init froze is gone", cap);
		else
			strscpy(buf, "keyring serial swap detected", cap);
	} else if (strcmp(v, "lsm-off") == 0) {
		strscpy(buf, "LSM not attached -- enforcement OFF", cap);
	} else if (strcmp(v, "init-pending") == 0) {
		strscpy(buf, "target hasn't been `shirudo init`'d", cap);
	} else if (strcmp(v, "reboot-pending") == 0) {
		strscpy(buf, "a config activation is pending until the next reboot",
			cap);
	} else if (strcmp(v, "degraded") == 0) {
		strscpy(buf, in->tpm_reachable ?
			"no register bound this boot" : "no TPM answered", cap);
	}
}
