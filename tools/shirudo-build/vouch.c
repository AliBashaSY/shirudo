// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * `shirudo-build keys vouch`: sign a vouch for another certificate.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <shirudo/misc.h>
#include <shirudo/string.h>

#include "bpm.h"
#include "util/crypto.h"
#include "util/hex.h"
#include "util/io.h"
#include "util/keys.h"
#include "util/log.h"
#include "util/malloc.h"
#include "util/paths.h"
#include "util/sign.h"

#define VOUCH_SIG_MAX	(64u * 1024u)

static void usage(FILE *fp)
{
	fprintf(fp,
		"usage: shirudo-build keys vouch <cert.der> [-o <file>]\n"
		"                [--keys-dir <dir>] [--key <signing_key.pem>]\n"
		"                [--cert <cert.der>]\n"
		"\n"
		"Sign a vouch for <cert.der> with the operator key, for\n"
		"`shirudo keys add --vouch` on the targets that enrol this\n"
		"key. -o defaults to <cert.der>.vouch.\n");
}

static int cert_algorithm_ok(const __u8 *der, size_t der_sz,
			     const char *what)
{
	struct cert cert = {};
	char name[64] = {};
	int ret;

	ret = cert_read_der(der, der_sz, &cert);
	if (ret) {
		pr_err("%s is not a certificate", what);
		return ret;
	}
	ret = cert_key_algorithm(&cert, name, sizeof(name));
	cert_free(&cert);
	if (ret || strcmp(name, "ML-DSA-87")) {
		pr_err("%s is %s; shirudo accepts only ML-DSA-87", what,
		       ret ? "of an unknown algorithm" : name);
		return -EKEYREJECTED;
	}
	return 0;
}

static int write_file(const char *path, const void *data, size_t len)
{
	int fd, ret;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0)
		return -errno;
	ret = write_complete(fd, data, len);
	if (close(fd) && !ret)
		ret = -errno;
	if (ret)
		unlink(path);
	return ret;
}

int build_keys_vouch(int argc, char **argv)
{
	const char *pkey = SIGNING_KEY_PATH, *cert = CERT_PATH;
	const char *keys_dir_arg = NULL, *input = NULL, *output = NULL;
	__u8 *der = NULL, *own = NULL, *payload = NULL;
	size_t der_sz = 0, own_sz = 0, payload_sz = 0;
	struct sign_io io = {};
	__u8 sha[32], fp[32], own_fp[32];
	int pkey_explicit = 0, cert_explicit = 0;
	char fp_hex[65], own_hex[65];
	char pkey_path[PATH_MAX];
	char cert_path[PATH_MAX];
	char out_path[PATH_MAX];
	const char *keys_dir;
	int ret = 1;

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];

		if ((!strcmp(a, "-o") || !strcmp(a, "--output")) &&
		    i + 1 < argc) {
			output = argv[++i];
		} else if (!strcmp(a, "--keys-dir") && i + 1 < argc) {
			keys_dir_arg = argv[++i];
		} else if (!strncmp(a, "--keys-dir=", 11)) {
			keys_dir_arg = a + 11;
		} else if (!strcmp(a, "--key") && i + 1 < argc) {
			pkey = argv[++i];
			pkey_explicit = 1;
		} else if (!strcmp(a, "--cert") && i + 1 < argc) {
			cert = argv[++i];
			cert_explicit = 1;
		} else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
			usage(stdout);
			return 0;
		} else if (a[0] == '-') {
			pr_err("keys vouch: unknown option '%s'", a);
			usage(stderr);
			return 2;
		} else if (!input) {
			input = a;
		} else {
			pr_err("keys vouch: unexpected argument '%s'", a);
			return 2;
		}
	}
	if (!input) {
		usage(stderr);
		return 2;
	}
	keys_dir = keys_dir_resolve(keys_dir_arg);
	if (keys_dir) {
		if (!pkey_explicit) {
			if (format_key_path(pkey_path, sizeof(pkey_path),
					    keys_dir,
					    SIGNING_KEY_FILE)) {
				pr_err("invalid keys dir %s", keys_dir);
				return 2;
			}
			pkey = pkey_path;
		}
		if (!cert_explicit) {
			if (format_key_path(cert_path, sizeof(cert_path),
					    keys_dir, CERT_FILE)) {
				pr_err("invalid keys dir %s", keys_dir);
				return 2;
			}
			cert = cert_path;
		}
	}
	if (build_check_keys(pkey, cert))
		return 1;
	if (!output) {
		if (strsprintf(out_path, sizeof(out_path), "%s.vouch", input) < 0) {
			pr_err("keys vouch: output path too long");
			return 2;
		}
		output = out_path;
	}

	ret = keys_read_der(input, &der, &der_sz);
	if (ret) {
		pr_err("keys vouch: read %s: %s", input, strerror(-ret));
		return 1;
	}
	ret = keys_digest_der(der, der_sz, sha, fp);
	if (ret) {
		pr_err("keys vouch: %s is not a certificate", input);
		goto out;
	}
	ret = cert_algorithm_ok(der, der_sz, input);
	if (ret)
		goto out;
	ret = keys_read_der(cert, &own, &own_sz);
	if (ret) {
		pr_err("keys vouch: read %s: %s", cert, strerror(-ret));
		goto out;
	}
	ret = keys_digest_der(own, own_sz, sha, own_fp);
	if (ret) {
		pr_err("keys vouch: %s is not a certificate", cert);
		goto out;
	}
	hex_encode(fp, 32, fp_hex);
	hex_encode(own_fp, 32, own_hex);
	if (!memcmp(fp, own_fp, 32)) {
		pr_err("keys vouch: %s holds the signing key itself; a key "
		       "cannot vouch for its own certificate", input);
		ret = -EINVAL;
		goto out;
	}
	ret = keys_vouch_payload(der, der_sz, &payload, &payload_sz);
	if (ret)
		goto out;
	io.insns = payload;
	io.insns_sz = payload_sz;
	io.signature_sz = VOUCH_SIG_MAX;
	io.signature = xmalloc(VOUCH_SIG_MAX);
	if (!io.signature) {
		ret = -ENOMEM;
		goto out;
	}
	ret = cms_sign(&io, pkey, cert);
	if (ret) {
		pr_err("keys vouch: sign: %s", strerror(-ret));
		goto out;
	}
	ret = write_file(output, io.signature, io.signature_sz);
	if (ret) {
		pr_err("keys vouch: write %s: %s", output, strerror(-ret));
		goto out;
	}
	pr_info("keys vouch: %s vouched for by %s, written to %s", fp_hex,
		own_hex, output);
	printf("%s\n", fp_hex);
out:
	xfree(io.signature, VOUCH_SIG_MAX);
	xfree(payload, payload_sz);
	xfree(own, own_sz);
	xfree(der, der_sz);
	return ret ? 1 : 0;
}
