// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Process-level hardening: deny ptrace, block core dumps, mlock
 * sensitive buffers.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

#include "harden.h"

/*
 * Started with standard input, output or error closed, the process gives
 * that number to the next file it opens, and what it prints lands in
 * that file: point each closed one at /dev/null before anything opens.
 */
void shirudo_harden_stdio(void)
{
	int fd;

	for (fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
		if (fcntl(fd, F_GETFD) != -1 || errno != EBADF)
			continue;
		/* The ones below fd are open, so fd is the lowest free. */
		if (open("/dev/null", O_RDWR) != fd)
			_exit(1);
	}
}

void shirudo_harden_self(void)
{
	struct rlimit no_core = { 0, 0 };

	prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	prctl(PR_SET_PTRACER, 0, 0, 0, 0);
	setrlimit(RLIMIT_CORE, &no_core);
}
