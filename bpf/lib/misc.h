/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Helpers shared by every BPF program.
 */

#ifndef SHIRUDO_BPF_MISC_H
#define SHIRUDO_BPF_MISC_H

/* Token for token as in include/shirudo/compiler.h; see there. */
#define BUILD_BUG_ON_MSG(cond, msg)	_Static_assert(!(cond), msg)
#define BUILD_BUG_ON(cond)						\
	BUILD_BUG_ON_MSG(cond, "BUILD_BUG_ON failed: " #cond)

#endif /* SHIRUDO_BPF_MISC_H */
