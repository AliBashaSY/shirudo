/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The kernel's fixed-width integer types, ptr_to_u64() and shirudo_trunc(),
 * for userspace.
 */

#ifndef SHIRUDO_MISC_H
#define SHIRUDO_MISC_H

#include <stdint.h>
#include <stdlib.h>

#include <shirudo/compiler.h>

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

/*
 * The C library's names for them, refused once <stdint.h> has defined
 * them and <stdlib.h> used them for random_r(), so that a later include
 * of either still works. Any other system header that uses them,
 * <netinet/in.h>, <elf.h> or one of OpenSSL's or tpm2-tss's, goes above
 * the first shirudo header.
 */
#pragma GCC poison uint8_t uint16_t uint32_t uint64_t
#pragma GCC poison int8_t int16_t int32_t int64_t

#endif /* SHIRUDO_MISC_H */
