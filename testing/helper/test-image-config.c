// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util/config.h"
#include "util/image_config.h"
#include "util/image_bootstrap.h"
#include "util/hex.h"

static int read_bootstrap(int argc, char **argv)
{
	struct shirudo_image_bootstrap boot = {};
	char root[65];
	int rc;

	rc = shirudo_image_bootstrap_read(argv[2], &boot, true);
	if (!rc && argc == 4)
		rc = shirudo_image_bootstrap_read(argv[3], &boot, false);
	if (!rc) {
		hex_encode(boot.runtime_root, 32, root);
		printf("runtime=%s\nroot=%s\nconfig=%s\ncertificate=%s\npcr=%u\n",
		       boot.runtime, root, boot.config, boot.certificate,
		       boot.pcr);
	}
	printf("rc=%d\n", rc);
	return rc ? 1 : 0;
}

int main(int argc, char **argv)
{
	struct shirudo_config bootstrap, effective;
	int fd, rc;

	if (argc < 3 || argc > 4)
		return 2;
	if (!strcmp(argv[1], "bootstrap"))
		return read_bootstrap(argc, argv);
	fd = open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return 1;
	rc = config_load_path(&bootstrap, "/dev/null");
	if (rc)
		goto out;
	memcpy(bootstrap.tpm_owner_auth, "bootstrap-owner", 15);
	bootstrap.tpm_owner_auth_len = 15;
	snprintf(bootstrap.tpm_tcti, sizeof(bootstrap.tpm_tcti),
		 "bootstrap-tcti");
	if (!strcmp(argv[1], "validate")) {
		rc = shirudo_image_config_validate(fd);
	} else if (!strcmp(argv[1], "effective")) {
		rc = shirudo_image_config_set(&bootstrap, fd);
		if (!rc && !shirudo_image_config_get(&effective))
			rc = -EIO;
		if (!rc) {
			printf("owner=%.*s\ntcti=%s\nlimit=%llu\n",
			       (int)effective.tpm_owner_auth_len,
			       effective.tpm_owner_auth, effective.tpm_tcti,
			       effective.eventlog_max_bytes);
			rc = shirudo_image_open(SHIRUDO_IMAGE_REGISTRIES,
						O_RDONLY | O_DIRECTORY);
			printf("registries=%s\n", rc >= 0 ? "open" : "missing");
			if (rc >= 0)
				close(rc);
			rc = 0;
		}
	} else if (!strcmp(argv[1], "relative") && argc == 4) {
		rc = shirudo_image_config_set(&bootstrap, fd);
		if (!rc)
			rc = shirudo_image_open_relative(argv[3], O_RDONLY);
		if (rc >= 0) {
			printf("resolved=open\n");
			close(rc);
			rc = 0;
		}
	} else {
		rc = -EINVAL;
	}
out:
	shirudo_image_config_clear();
	close(fd);
	printf("rc=%d\n", rc);
	return rc ? 1 : 0;
}
