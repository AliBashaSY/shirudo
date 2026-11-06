// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * kernel_build_id_from_notes() of util/kernel_build_id.c on every note
 * section of LEN bytes: it reads nothing outside the section, a refusal
 * leaves an empty string, and a build ID is the lowercase hex of at most
 * 64 bytes, that of the first note when it is the GNU build ID, type 3
 * named "GNU", and none from a section whose one note is anything else.
 */

#include "proof.h"
#include "../../../util/hex.c"
#include "../../../util/kernel_build_id.c"

static __u32 word(const __u8 *p)
{
	__u32 v;

	memcpy(&v, p, sizeof(v));
	return v;
}

static bool lower_hex(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static bool gnu_build_id(const __u8 *notes)
{
	return word(notes + 8) == 3 && word(notes) == 4 &&
	       !memcmp(notes + 12, "GNU", 4);
}

/* A section of one note, of a name of up to 4 bytes and a descriptor. */
static bool one_note(const __u8 *notes)
{
	__u32 namesz = word(notes), descsz = word(notes + 4);

	return LEN >= 12 && namesz <= 4 && descsz <= LEN &&
	       12 + align4(namesz) + align4(descsz) == LEN;
}

static void first_note(const __u8 *notes, const char *out)
{
	static const char d[] = "0123456789abcdef";
	__u32 namesz, descsz;
	size_t desc, i;

	if (LEN < 16)
		return;
	namesz = word(notes);
	descsz = word(notes + 4);
	if (!gnu_build_id(notes) || !descsz || descsz > LEN)
		return;
	desc = 12 + align4(namesz);
	if (desc + align4(descsz) > LEN)
		return;
	for (i = 0; i < descsz; i++)
		__CPROVER_assert(out[2 * i] == d[notes[desc + i] >> 4] &&
				 out[2 * i + 1] == d[notes[desc + i] & 15],
				 "the first note's build ID");
	__CPROVER_assert(!out[2 * descsz], "and nothing more");
}

void harness(void)
{
	char out[KERNEL_BUILD_ID_STRING_MAX];
	__u8 *notes;
	size_t i, n;

	notes = any_bytes(LEN);
	if (kernel_build_id_from_notes(notes, LEN, out)) {
		__CPROVER_assert(!out[0], "a refusal leaves an empty string");
		return;
	}
	__CPROVER_assert(!one_note(notes) || gnu_build_id(notes),
			 "only a GNU build ID note gives one");
	n = strnlen(out, sizeof(out));
	__CPROVER_assert(n < sizeof(out) && n >= 2 && !(n % 2),
			 "a terminated ID of whole bytes, at most 64");
	for (i = 0; i < n; i++)
		__CPROVER_assert(lower_hex(out[i]), "in lowercase hex");
	first_note(notes, out);
}
