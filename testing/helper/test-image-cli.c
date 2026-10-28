// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include "../../builtin-bpm.c"
#include "util/config.h"

int __wrap_bpm_tree_fetch_publication(struct bpm_oci_client *client,
				      const char *repository,
				      const char *reference,
				      const char *certificate,
				      struct shirudo_tree_publication *pub,
				      char *digest, size_t capacity);

int __wrap_bpm_tree_fetch_publication(struct bpm_oci_client *client,
				      const char *repository,
				      const char *reference,
				      const char *certificate,
				      struct shirudo_tree_publication *pub,
				      char *digest, size_t capacity)
{
	(void)client;
	(void)repository;
	(void)reference;
	(void)pub;
	(void)digest;
	(void)capacity;
	puts(certificate);
	return 0;
}

int main(int argc, const char **argv)
{
	struct shirudo_config bootstrap;
	int fd, rc;

	if (argc < 3)
		return 2;
	fd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0)
		return 1;
	rc = config_load_path(&bootstrap, "/dev/null");
	if (!rc) {
		memcpy(bootstrap.tpm_owner_auth, "bootstrap-owner", 15);
		bootstrap.tpm_owner_auth_len = 15;
		strscpy(bootstrap.tpm_tcti, "bootstrap-tcti", sizeof(bootstrap.tpm_tcti));
		rc = shirudo_image_config_set(&bootstrap, fd);
	}
	close(fd);
	if (rc)
		return 1;
	if (!strcmp(argv[2], "bpm")) {
		rc = cmd_bpm(argc - 2, argv + 2);
	} else if (!strcmp(argv[2], "keys")) {
		rc = cmd_keys(argc - 2, argv + 2);
	} else if (!strcmp(argv[2], "config")) {
		rc = cmd_config(argc - 2, argv + 2);
	} else if (!strcmp(argv[2], "source") && argc == 4) {
		struct bpm_source source;

		rc = load_source(argv[3], &source);
		if (!rc) {
			char content[64];
			int kfd = shirudo_image_open_relative(source.key_file,
							      O_RDONLY);
			FILE *file = kfd < 0 ? NULL : fdopen(kfd, "r");

			if (!file) {
				rc = kfd < 0 ? kfd : -errno;
				if (kfd >= 0)
					close(kfd);
			} else {
				if (!fgets(content, sizeof(content), file))
					rc = -EIO;
				else
					fputs(content, stdout);
				fclose(file);
			}
		}
	} else if (!strcmp(argv[2], "tree")) {
		struct shirudo_tree_publication publication = {};
		struct bpm_tree_config config = {};
		char digest[BPM_MAX_DIGEST];

		strscpy(config.endpoint, "oci://example.invalid/tree", sizeof(config.endpoint));
		strscpy(config.reference, "latest", sizeof(config.reference));
		rc = tree_fetch_publication(&config, &publication, digest,
					    sizeof(digest));
	} else {
		rc = 2;
	}
	shirudo_image_config_clear();
	return rc ? 1 : 0;
}
