// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <shirudo/misc.h>

#include "../../util/upatch_rodata.c"

/*
 * A typedef of len characters behind ptrs pointers, named into a buffer
 * of cap bytes: the result, and whether the buffer kept its terminator.
 */
static int name_into(size_t len, int ptrs, size_t cap)
{
	char name[512], buf[512];
	struct btf *btf;
	size_t off = 0;
	int id, rc;

	if (len >= sizeof(name) || cap > sizeof(buf))
		return 2;
	memset(name, 'n', len);
	name[len] = '\0';
	memset(buf, 'x', sizeof(buf));
	btf = btf__new_empty();
	if (!btf)
		return 2;
	id = btf__add_int(btf, "int", 4, BTF_INT_SIGNED);
	if (id > 0)
		id = btf__add_typedef(btf, name, id);
	while (id > 0 && ptrs--)
		id = btf__add_ptr(btf, id);
	if (id <= 0) {
		btf__free(btf);
		return 2;
	}
	rc = append_type_name(btf, (__u32)id, buf, cap, &off, 0);
	btf__free(btf);
	printf("rc=%d off=%zu terminated=%d\n", rc, off,
	       !!memchr(buf, '\0', cap));
	if (!memchr(buf, '\0', cap))
		return 1;
	return rc ? 3 : 0;
}

int main(int argc, char **argv)
{
	__u64 len, ptrs;

	if (argc != 3 || parse_u64(argv[1], strlen(argv[1]), 500, &len) ||
	    parse_u64(argv[2], strlen(argv[2]), 4, &ptrs))
		return 2;
	return name_into((size_t)len, (int)ptrs, 256);
}
