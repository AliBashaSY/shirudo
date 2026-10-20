/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Compact durations: 12s, 5m, 3h, 7d.
 */

#ifndef SHIRUDO_TIMEFMT_H
#define SHIRUDO_TIMEFMT_H

#include <stddef.h>

/* A negative seconds writes "?"; cap must hold the worst-case %ldd. */
void shirudo_format_duration(long seconds, char *buf, size_t cap);

#endif /* SHIRUDO_TIMEFMT_H */
