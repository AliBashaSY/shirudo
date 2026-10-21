/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The kernel's fixed-width integer types, ptr_to_u64() and shirudo_trunc(),
 * for userspace.
 */

#ifndef SHIRUDO_MISC_H
#define SHIRUDO_MISC_H

/*
 * Defined as asm-generic/int-ll64.h defines them on x86-64, arm64 and
 * riscv64, so a file that also includes <linux/types.h> sees the same
 * types twice, which C11 allows.
 */
typedef signed char		__s8;
typedef unsigned char		__u8;
typedef signed short		__s16;
typedef unsigned short		__u16;
typedef signed int		__s32;
typedef unsigned int		__u32;
typedef signed long long	__s64;
typedef unsigned long long	__u64;

/* A pointer as the __u64 the bpf() ABI carries it in. */
static inline __u64 ptr_to_u64(const void *ptr)
{
	return (__u64)(unsigned long)ptr;
}

/*
 * trunc() without libm, which clang calls for it and the binaries do
 * not link. A double of 2^52 or more is integral already.
 */
static inline double shirudo_trunc(double x)
{
	if (x == 0 || !(x > -0x1p52 && x < 0x1p52))
		return x;
	if (x < 0)
		return -(double)(__s64)-x;
	return (double)(__s64)x;
}

#endif
