/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2026 Authors of shirudo */

#ifndef SHIRUDO_MEASURED_H
#define SHIRUDO_MEASURED_H

/*
 * The upatches pinned in the private bpffs that this boot's records do
 * not measure as loaded with the payload their pin records: what a load
 * stopped between its attach and its extend leaves, or an unload stopped
 * after its extend. cb runs for each with its name, and a non-zero
 * return ends the walk with that value. The number found, or -errno
 * when the pins or the records cannot be read; -ENOENT when this boot
 * has no record.
 */
int measured_unmeasured_pins(int (*cb)(const char *name, void *ctx),
			     void *ctx);

#endif /* SHIRUDO_MEASURED_H */
