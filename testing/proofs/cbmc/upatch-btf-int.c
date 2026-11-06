// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The integers util/upatch_rodata.c reads from the .rodata of a .upatch by
 * their BTF encoding, for every encoding and value of up to 8 bytes: it
 * reads only the integer's bytes and returns a value of its bits.
 */

#include "proof.h"
#include "../../../util/upatch_rodata.c"

void harness(void)
{
	struct {
		struct btf_type t;
		__u32 enc;
	} it;
	bool is_signed, is_bool;
	__u32 bits, size;
	__u64 value;
	__u8 *bytes;

	__CPROVER_assume(size <= sizeof(value));
	bytes = any_bytes(size);
	if (btf_integer(&it.t, bytes, size, &value, &is_signed, &is_bool))
		return;
	bits = BTF_INT_BITS(it.enc);
	__CPROVER_assert(it.t.size && it.t.size <= size && bits &&
			 BTF_INT_OFFSET(it.enc) + bits <= it.t.size * 8,
			 "bits inside the integer's bytes");
	if (bits == 64)
		return;
	if (!is_signed)
		__CPROVER_assert(!(value >> bits),
				 "an unsigned value of its bits");
	else
		__CPROVER_assert(!(value >> (bits - 1)) ||
				 !~(value | ((1ULL << (bits - 1)) - 1)),
				 "a signed value of its bits");
}
