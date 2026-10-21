// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * shirudo_upatch_open() on a .upatch container, and its decoder table.
 */

#include "fuzz.h"
#include "upatch.h"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct shirudo_prog_spec *specs;
	struct shirudo_upatch u;
	char path[64];
	int fd;

	fd = fuzz_file(data, size, path, sizeof(path));
	if (fd < 0)
		return 0;
	if (!shirudo_upatch_open(path, &u)) {
		if (!shirudo_prog_specs_parse(shirudo_upatch_metadata(&u),
					      u.hdr.metadata_len,
					      u.hdr.nr_progs, &specs))
			shirudo_prog_specs_free(specs, u.hdr.nr_progs);
		shirudo_upatch_is_unload(&u);
		if (shirudo_upatch_has_decoders(&u)) {
			struct shirudo_upatch_decoder_view d;

			for (__u32 i = 0; !shirudo_upatch_decoder_view(&u, i, &d); i++)
				;
		}
		shirudo_upatch_close(&u);
	}
	close(fd);
	return 0;
}
