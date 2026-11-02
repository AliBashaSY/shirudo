/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Project-wide macros. Boilerplate, uncopyrightable.
 */

#ifndef SHIRUDO_MACROS_H
#define SHIRUDO_MACROS_H

#include <shirudo/compiler.h>

/*
 * ARRAY_SIZE() refuses a pointer: __builtin_types_compatible_p tells
 * T[N] from T*.
 */
#define ARRAY_SIZE(arr)							\
	(sizeof(arr) / sizeof((arr)[0]) +				\
	 BUILD_BUG_ON_ZERO(					\
		 __builtin_types_compatible_p(typeof(arr),		\
					      typeof(&(arr)[0]))))

#endif /* SHIRUDO_MACROS_H */
