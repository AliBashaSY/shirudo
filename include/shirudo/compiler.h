/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Attributes and compile-time checks spelled the kernel's way.
 */

#ifndef SHIRUDO_COMPILER_H
#define SHIRUDO_COMPILER_H

#define __printf(a, b)	__attribute__((format(printf, a, b)))
#define __must_check	__attribute__((warn_unused_result))
#define __noreturn	__attribute__((noreturn))

/*
 * Declarations, so they stand at file scope as well as in a block.
 * Token for token as in bpf/lib/misc.h, which userspace also reaches
 * through bpf/lib/serializer.h: a redefinition that differs does not
 * compile.
 */
#define BUILD_BUG_ON_MSG(cond, msg)	_Static_assert(!(cond), msg)
#define BUILD_BUG_ON(cond)						\
	BUILD_BUG_ON_MSG(cond, "BUILD_BUG_ON failed: " #cond)

/* 0 as an int constant expression, or no build when e holds. */
#define BUILD_BUG_ON_ZERO(e)						\
	((int)(0 * sizeof(struct { _Static_assert(!(e), #e); char c; })))

#endif /* SHIRUDO_COMPILER_H */
