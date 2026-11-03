/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2026 Authors of shirudo */

#ifndef SHIRUDO_MEASURED_H
#define SHIRUDO_MEASURED_H

#include <stdbool.h>

/*
 * The set of upatches the eventlog records as loaded, each with the
 * payload of its load: an init record's set, with every later load
 * added and every unload removed, in order. measured_set_boot() folds
 * this boot's records and is -ENOENT when there are none;
 * measured_set_log() folds every boot's, an empty set when there is no
 * log yet.
 */
struct measured_set;

int measured_set_boot(struct measured_set **out);
int measured_set_log(struct measured_set **out);
bool measured_holds(const struct measured_set *s, const char *name,
		    const char *payload_hex);
void measured_set_free(struct measured_set *s);

/*
 * The upatches pinned in the private bpffs, or archived with the seal
 * label, that the set does not hold with the payload their pin or
 * archive records: what a load stopped between its pins and its extend
 * leaves, an unload stopped after its extend, or an archive root
 * brought back after its unload. cb runs for each with its name, and
 * the archive's path, and a non-zero return ends the walk with that
 * value. The number found, or -errno when the pins or archives cannot
 * be read.
 */
int measured_unmeasured_pins(const struct measured_set *s,
			     int (*cb)(const char *name, void *ctx),
			     void *ctx);
int measured_unmeasured_archives(const struct measured_set *s,
				 int (*cb)(const char *name,
					   const char *path, void *ctx),
				 void *ctx);

#endif /* SHIRUDO_MEASURED_H */
