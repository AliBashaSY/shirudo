/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2026 Authors of shirudo */

#ifndef SHIRUDO_IO_H
#define SHIRUDO_IO_H

#include <stddef.h>

int shirudo_write_all(int fd, const void *buf, size_t len);

/*
 * A regular file of at most max bytes, into *out xmalloc'd to *len + 1
 * bytes and NUL-terminated; out is the address of a char or unsigned
 * char pointer.
 */
int shirudo_read_file(const char *path, size_t max, void *out, size_t *len);

#endif /* SHIRUDO_IO_H */
