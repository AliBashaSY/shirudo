// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * upatch_open() on a .upatch container, and its decoder table.
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
	if (!upatch_open(path, &u)) {
		if (!prog_specs_parse(upatch_metadata(&u),
				      u.hdr.metadata_len,
				      u.hdr.nr_progs, &specs))
			prog_specs_free(specs, u.hdr.nr_progs);
		upatch_is_unload(&u);
		if (upatch_has_decoders(&u)) {
			struct upatch_decoder_view d;

			for (__u32 i = 0; !upatch_decoder_view(&u, i, &d); i++)
				;
		}
		upatch_close(&u);
	}
	close(fd);
	return 0;
}
