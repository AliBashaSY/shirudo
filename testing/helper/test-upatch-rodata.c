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
	int id, ret;

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
	ret = append_type_name(btf, (__u32)id, buf, cap, &off, 0);
	btf__free(btf);
	printf("rc=%d off=%zu terminated=%d\n", ret, off,
	       !!memchr(buf, '\0', cap));
	if (!memchr(buf, '\0', cap))
		return 1;
	return ret ? 3 : 0;
}

/*
 * A struct of an int, an enum and a pointer, spoiled as what says,
 * through the walk that guards libbpf's dumper: 0 when it may be dumped,
 * 1 when it is refused.
 */
static int dumpable(const char *what)
{
	const char *member = "a", *value = "E", *pointee = "int";
	int ints, en, target, ptr, st, ret = 2;
	struct btf_type *t;
	struct btf *btf;
	size_t done_len;
	__u8 *done;

	if (!strcmp(what, "member"))
		member = "a\x1b[2J";
	else if (!strcmp(what, "enum"))
		value = "E\x07";
	else if (!strcmp(what, "pointee"))
		pointee = "int\xc2\x9b";
	btf = btf__new_empty();
	if (!btf)
		return 2;
	ints = btf__add_int(btf, "int", 4, BTF_INT_SIGNED);
	target = btf__add_int(btf, pointee, 4, BTF_INT_SIGNED);
	en = btf__add_enum(btf, "e", 4);
	if (en > 0 && btf__add_enum_value(btf, value, 0))
		en = -1;
	if (!strcmp(what, "proto")) {
		target = btf__add_func_proto(btf, ints);
		if (target > 0 && btf__add_func_param(btf, "x", ints))
			target = -1;
	}
	ptr = btf__add_ptr(btf, target);
	if (ptr > 0 && !strcmp(what, "loop")) {
		t = (struct btf_type *)btf__type_by_id(btf, ptr);
		t->type = ptr;
	}
	st = btf__add_struct(btf, "s", 16);
	if (ints <= 0 || target <= 0 || en <= 0 || ptr <= 0 || st <= 0 ||
	    btf__add_field(btf, member, ints, 0, 0) ||
	    btf__add_field(btf, "b", en, 32, 0) ||
	    btf__add_field(btf, "p", ptr, 64, 0))
		goto out;
	done_len = btf__type_cnt(btf) / 8 + 1;
	done = xcalloc(done_len, 1);
	if (!done)
		goto out;
	ret = btf_dumpable(btf, (__u32)st, 0, done) ? 0 : 1;
	printf("%s dumpable=%d\n", what, !ret);
	xfree(done, done_len);
out:
	btf__free(btf);
	return ret;
}

int main(int argc, char **argv)
{
	__u64 len, ptrs;

	if (argc == 3 && !strcmp(argv[1], "dumpable"))
		return dumpable(argv[2]);
	if (argc != 3 || parse_u64(argv[1], strlen(argv[1]), 500, &len) ||
	    parse_u64(argv[2], strlen(argv[2]), 4, &ptrs))
		return 2;
	return name_into((size_t)len, (int)ptrs, 256);
}
