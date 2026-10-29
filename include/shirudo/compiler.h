/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Function attributes spelled the kernel's way.
 */

#ifndef SHIRUDO_COMPILER_H
#define SHIRUDO_COMPILER_H

#define __printf(a, b)	__attribute__((format(printf, a, b)))

#endif /* SHIRUDO_COMPILER_H */
