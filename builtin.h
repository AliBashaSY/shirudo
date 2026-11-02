/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Subcommand entry points dispatched from shirudo.c.
 */

#ifndef SHIRUDO_BUILTIN_H
#define SHIRUDO_BUILTIN_H

#include <stdbool.h>

/* Operator-side. */
int cmd_genkey(int argc, const char **argv);
int cmd_build(int argc, const char **argv);

/*
 * The operator package's helper: whether it is there, and the one exec
 * site, which returns only on failure.
 */
bool shirudo_build_available(void);
int shirudo_build_exec(const char *label, char **argv);

/* Target-side. */
int cmd_init(int argc, const char **argv);
int cmd_upatch(int argc, const char **argv);
int cmd_bpm(int argc, const char **argv);

/* Either-side. */
int cmd_status(int argc, const char **argv);
int cmd_attest(int argc, const char **argv);
int cmd_version(int argc, const char **argv);
int cmd_keys(int argc, const char **argv);
int cmd_config(int argc, const char **argv);
int cmd_image(int argc, const char **argv);

#endif /* SHIRUDO_BUILTIN_H */
