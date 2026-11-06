// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * set_owner_auth() of util/config.c, the shirudo.conf reader, on every
 * tpm.owner_auth value of up to LEN bytes and any earlier value: it stores
 * the bytes an even run of hex digits after "hex:" spells, or the text
 * itself, and a refusal leaves the earlier value intact.
 */

#define PROOF_MEMCPY_BYTEWISE
#include "proof.h"
#include "../../../util/string.c"
#include "../../../util/hex.c"
#include "../../../util/config.c"

static struct config cfg;

void pr_warn(const char *fmt, ...)
{
}

static char *any_string(void)
{
	char *s = any_bytes(LEN + 1);

	s[LEN] = '\0';
	return s;
}

static bool hex_run(const char *s, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (hex_nibble(s[i]) < 0)
			return false;
	return !(n % 2);
}

void harness(void)
{
	char *val = any_string(), before[LEN + 1];
	size_t had, n, i;

	__CPROVER_assume(had <= sizeof(cfg.tpm_owner_auth));
	cfg.tpm_owner_auth_len = had;
	for (i = 0; i < LEN; i++)
		before[i] = cfg.tpm_owner_auth[i];
	n = strlen(val);
	set_owner_auth(&cfg, val);
	if (strncmp(val, "hex:", 4)) {
		__CPROVER_assert(cfg.tpm_owner_auth_len == n &&
				 !memneq_explicit(cfg.tpm_owner_auth, val, n),
				 "a passphrase as it stands");
		return;
	}
	if (!hex_run(val + 4, n - 4)) {
		__CPROVER_assert(cfg.tpm_owner_auth_len == had &&
				 !memneq_explicit(cfg.tpm_owner_auth, before, LEN),
				 "a refusal leaves the earlier value");
		return;
	}
	__CPROVER_assert(cfg.tpm_owner_auth_len == (n - 4) / 2,
			 "the bytes the hex digits spell");
	for (i = 0; i < (n - 4) / 2; i++)
		__CPROVER_assert((__u8)cfg.tpm_owner_auth[i] ==
				 (hex_nibble(val[4 + 2 * i]) << 4 |
				  hex_nibble(val[5 + 2 * i])),
				 "each one");
}
