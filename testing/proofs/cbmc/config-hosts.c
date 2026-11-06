// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * tls_classical_hosts_valid() of util/config.c, the shirudo.conf reader, on
 * every tls.classical_hosts value of up to LEN bytes: what it passes is
 * empty or a comma-separated list of names dns_name_valid() takes.
 */

#define PROOF_MEMCPY_BYTEWISE
#include "proof.h"
#include "../../../util/string.c"
#include "../../../util/hex.c"
#include "../../../util/config.c"

void pr_warn(const char *fmt, ...)
{
}

static char *any_string(void)
{
	char *s = any_bytes(LEN + 1);

	s[LEN] = '\0';
	return s;
}

void harness(void)
{
	char *val = any_string();
	size_t start, i;

	if (!tls_classical_hosts_valid(val) || !*val)
		return;
	for (start = 0; ; start = i + 1) {
		char name[LEN + 1] = {};

		for (i = start; val[i] && val[i] != ','; i++)
			name[i - start] = val[i];
		__CPROVER_assert(dns_name_valid(name), "DNS names");
		if (!val[i])
			break;
	}
}
