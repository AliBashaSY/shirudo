/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The structured help every subcommand prints.
 */

#ifndef SHIRUDO_HELP_H
#define SHIRUDO_HELP_H

#include <stddef.h>

struct help_item {
	const char	*name;
	const char	*summary;
};

struct help_section {
	const char			*header;
	const struct help_item	*items;
	size_t				n_items;
};

struct help {
	const char				*usage;
	const char				*summary;
	const struct help_section	*sections;
	size_t					n_sections;
	const char				*footer;
};

int is_help(const char *arg);

int is_list_cmds(const char *arg);

void print_help(const struct help *h);

/* What bash completion reads through --list-cmds. */
void print_subcmd_list(const struct help_item *items,
		       size_t n_items);

/*
 * NULL on failure with stderr already written: a did-you-mean block,
 * or the ambiguous matches.
 */
const char *resolve_subcommand(const char *cmd_label,
			       const char *input,
			       const struct help_item *subcmds,
			       size_t n_subcmds);

#endif /* SHIRUDO_HELP_H */
