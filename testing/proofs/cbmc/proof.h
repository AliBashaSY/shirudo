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

#ifdef PROOF_MEMCHR_QUANTIFIED
/*
 * For ranges of thousands of bytes, which a loop unrolls too slowly, where
 * the caller asks whether a byte is there and not where: NULL exactly when
 * none of the n bytes matches, n a constant, and otherwise any match. CBMC
 * ignores a quantifier over a range it cannot bound ("warning: ignoring
 * forall"), and then this may also return NULL where a byte matches.
 */
void *memchr(const void *s, int c, size_t n)
{
	const __u8 *p = s;
	size_t i;

	__CPROVER_assert(__CPROVER_r_ok(s, n), "memchr() reads its range");
	if (__CPROVER_forall { size_t j; j >= n || p[j] != (__u8)c })
		return NULL;
	__CPROVER_assume(i < n && p[i] == (__u8)c);
	return (void *)(p + i);
}
#else
void *memchr(const void *s, int c, size_t n)
{
	const __u8 *p = s;
	size_t i;

	for (i = 0; i < n; i++)
		if (p[i] == (__u8)c)
			return (void *)(p + i);
	return NULL;
}
#endif

/*
 * CBMC 5.95's memcpy() reads wrong bytes from a string literal when the
 * source pointer is one of several literals picked by symbolic data, and
 * so fails a proof the code meets. A harness that copies from such a
 * pointer defines PROOF_MEMCPY_BYTEWISE before including this header.
 */
#ifdef PROOF_MEMCPY_BYTEWISE
void *memcpy(void *dst, const void *src, size_t n)
{
	const __u8 *s = src;
	__u8 *d = dst;
	size_t i;

	for (i = 0; i < n; i++)
		d[i] = s[i];
	return dst;
}
#endif

static inline void *any_bytes(size_t size)
{
	void *p = __CPROVER_allocate(size, 0);

	__CPROVER_assume(p);
	return p;
}

#endif /* SHIRUDO_PROOF_H */
