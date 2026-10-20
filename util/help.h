/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The structured help every subcommand prints.
 */

#ifndef SHIRUDO_HELP_H
#define SHIRUDO_HELP_H

#include <stddef.h>

struct shirudo_help_item {
	const char	*name;
	const char	*summary;
};

struct shirudo_help_section {
	const char			*header;
	const struct shirudo_help_item	*items;
	size_t				n_items;
};

struct shirudo_help {
	const char				*usage;
	const char				*summary;
	const struct shirudo_help_section	*sections;
	size_t					n_sections;
	const char				*footer;
};

int shirudo_is_help(const char *arg);

int shirudo_is_list_cmds(const char *arg);

void shirudo_print_help(const struct shirudo_help *h);

/* What bash completion reads through --list-cmds. */
void shirudo_print_subcmd_list(const struct shirudo_help_item *items,
			       size_t n_items);

void shirudo_help_unknown_subcommand(const char *cmd_label,
				     const char *bad,
				     const struct shirudo_help_item *subcmds,
				     size_t n_subcmds);

/*
 * NULL on failure with stderr already written: a did-you-mean block,
 * or the ambiguous matches.
 */
const char *shirudo_resolve_subcommand(const char *cmd_label,
				       const char *input,
				       const struct shirudo_help_item *subcmds,
				       size_t n_subcmds);

#endif /* SHIRUDO_HELP_H */
