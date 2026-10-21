// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Focused tests for target-local BPM source and upatch tree configuration
 * parsing.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "util/bpm_source_config.h"

int main(void)
{
	char tree_endpoint_line[] = " endpoint=\"oci://registry.test/tree\"\n";
	char key_file_line[] = "key_file = /etc/shirudo/bpm/keys/jbrust.pem\n";
	char endpoint_line[] = "endpoint = oci://registry.test:5443\n";
	char unbalanced_line[] = "reference = \"sha256:feed\n";
	char reference_line[] = "reference = sha256:feed\r\n";
	char key_file[256] = {};
	char reference[80] = {};
	char endpoint[512] = {};
	char long_line[512];

	if (bpm_parse_source_line(endpoint_line, endpoint,
				  sizeof(endpoint), key_file,
				  sizeof(key_file)) ||
	    bpm_parse_source_line(key_file_line, endpoint,
				  sizeof(endpoint), key_file,
				  sizeof(key_file)))
		return 1;
	if (strcmp(endpoint, "oci://registry.test:5443") ||
	    strcmp(key_file, "/etc/shirudo/bpm/keys/jbrust.pem"))
		return 1;

	printf("endpoint=%s\nkey_file=%s\n", endpoint, key_file);

	if (bpm_parse_tree_line(tree_endpoint_line, endpoint,
				sizeof(endpoint), reference,
				sizeof(reference)) ||
	    bpm_parse_tree_line(reference_line, endpoint,
				sizeof(endpoint), reference,
				sizeof(reference)) ||
	    bpm_parse_tree_line(unbalanced_line, endpoint,
				sizeof(endpoint), reference,
				sizeof(reference)) != -EINVAL)
		return 1;
	printf("tree endpoint=%s\ntree reference=%s\n", endpoint, reference);
	/* A value that does not fit is refused, not cut. */
	snprintf(long_line, sizeof(long_line), "reference = sha256:%0*d\n",
		 (int)sizeof(reference), 0);
	if (bpm_parse_tree_line(long_line, endpoint, sizeof(endpoint),
				reference, sizeof(reference)) != -E2BIG)
		return 1;
	snprintf(long_line, sizeof(long_line), "key_file = /%0*d\n",
		 (int)sizeof(key_file), 0);
	if (bpm_parse_source_line(long_line, endpoint, sizeof(endpoint),
				  key_file, sizeof(key_file)) != -E2BIG)
		return 1;
	return 0;
}
