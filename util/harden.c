// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Process-level hardening: deny ptrace, block core dumps, mlock
 * sensitive buffers.
 */

#include <sys/prctl.h>
#include <sys/resource.h>

#include "harden.h"

void shirudo_harden_self(void)
{
	struct rlimit no_core = { 0, 0 };

	prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	prctl(PR_SET_PTRACER, 0, 0, 0, 0);
	setrlimit(RLIMIT_CORE, &no_core);
}
