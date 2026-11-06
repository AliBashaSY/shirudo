/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * What the CBMC harnesses share: memory of any contents, which the
 * allocators of util/malloc.h cannot give as they zero what they hand out.
 */

#ifndef SHIRUDO_PROOF_H
#define SHIRUDO_PROOF_H

#include <stddef.h>

#include <shirudo/types.h>

/*
 * CBMC models neither strnlen() nor memchr(), and a function without a
 * body returns anything.
 */
size_t strnlen(const char *s, size_t max)
{
	size_t n = 0;

	while (n < max && s[n])
		n++;
	return n;
}

void *memchr(const void *s, int c, size_t n)
{
	const __u8 *p = s;
	size_t i;

	for (i = 0; i < n; i++)
		if (p[i] == (__u8)c)
			return (void *)(p + i);
	return NULL;
}

static inline void *any_bytes(size_t size)
{
	void *p = __CPROVER_allocate(size, 0);

	__CPROVER_assume(p);
	return p;
}

#endif /* SHIRUDO_PROOF_H */
