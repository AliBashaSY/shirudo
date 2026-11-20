// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <shirudo/string.h>

#include "bpm_oci.h"
#include "io.h"
#include "unit.h"

#define DIGEST "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define MEDIA "\"mediaType\":\"application/vnd.shirudo.upatch.v1\""

static int manifest_of(const char *layers, int *n)
{
	struct bpm_descriptor descs[4];
	char text[1024];

	ASSERT_TRUE(strsprintf(text, sizeof(text),
			       "{\"schemaVersion\":2,\"layers\":[%s]}",
			       layers) > 0);
	*n = -1;
	return bpm_oci_parse_manifest((const __u8 *)text, strlen(text), descs,
				      ARRAY_SIZE(descs), n, NULL, 0);
}

/*
 * The registry writes the manifest before anything is verified: a layer
 * without its digest or with a negative size would be fetched from an
 * empty URL or skip the size check.
 */
static void manifest_layer_is_a_whole_descriptor(void)
{
	int n;

	ASSERT_EQ(manifest_of("{" MEDIA ",\"digest\":\"" DIGEST "\","
			      "\"size\":5}", &n), 0);
	EXPECT_EQ(n, 1);
	EXPECT_EQ(manifest_of("{" MEDIA ",\"size\":5}", &n), -EINVAL);
	EXPECT_EQ(manifest_of("{" MEDIA ",\"digest\":\"sha512:aa\","
			      "\"size\":5}", &n), -EINVAL);
	EXPECT_EQ(manifest_of("{" MEDIA ",\"digest\":\"" DIGEST "\"}", &n),
		  -EINVAL);
	EXPECT_EQ(manifest_of("{" MEDIA ",\"digest\":\"" DIGEST "\","
			      "\"size\":-5}", &n), -EINVAL);
	EXPECT_EQ(manifest_of("{\"digest\":\"" DIGEST "\",\"size\":5}", &n),
		  -EINVAL);
	EXPECT_EQ(manifest_of("{" MEDIA ",\"digest\":\"" DIGEST "\","
			      "\"size\":5},{" MEDIA ",\"size\":5}", &n),
		  -EINVAL);
}

/* SHA-256 of "abc", FIPS 180-2 appendix B.1. */
#define ABC_DIGEST \
	"sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"

static void digest_names_a_blob_as_a_registry_does(void)
{
	char out[80];

	memset(out, 'x', sizeof(out));
	ASSERT_EQ(bpm_sha256_digest("abc", 3, out, 72), 0);
	EXPECT_STREQ(out, ABC_DIGEST);
	EXPECT_EQ(bpm_sha256_digest("abc", 3, out, 71), -ENAMETOOLONG);
}

#define REGISTRY	"registry.example:5000"
/* "user:pass" */
#define AUTHS		"{\"auths\":{\"" REGISTRY "\":{\"auth\":\"dXNlcjpwYXNz\"}}}"

/* The credentials shirudo takes for REGISTRY from an auth file of len bytes. */
static int auth_file(const char *text, size_t len, struct bpm_oci_client *c)
{
	const char *tmp = getenv("TMPDIR");
	char path[4096];
	int fd, ret;

	ASSERT_TRUE(strsprintf(path, sizeof(path), "%s/shirudo-unit-auth-XXXXXX",
			       tmp && tmp[0] ? tmp : "/tmp") > 0);
	fd = mkstemp(path);
	ASSERT_TRUE(fd >= 0);
	ASSERT_EQ(write_complete(fd, text, len), 0);
	ASSERT_EQ(close(fd), 0);
	ASSERT_EQ(setenv("REGISTRY_AUTH_FILE", path, 1), 0);
	memset(c, 0, sizeof(*c));
	ret = bpm_oci_parse_endpoint("oci://" REGISTRY, c);
	ASSERT_EQ(unsetenv("REGISTRY_AUTH_FILE"), 0);
	ASSERT_EQ(unlink(path), 0);
	return ret;
}

/*
 * The file is searched as a string. A NUL ahead of the registry's entry
 * left the request without credentials and without a word, and one
 * behind it took the entry from a file that is no text.
 */
static void auth_file_with_a_nul_is_refused(void)
{
	static const char ahead[] = "\0" AUTHS;
	static const char behind[] = AUTHS "\0{";
	struct bpm_oci_client c __cleanup(bpm_oci_client_clear) = {};

	ASSERT_EQ(unsetenv("BPM_OCI_USERNAME"), 0);
	ASSERT_EQ(unsetenv("BPM_OCI_SCHEME"), 0);
	ASSERT_EQ(unsetenv("BPM_OCI_CA_FILE"), 0);
	ASSERT_EQ(auth_file(AUTHS, sizeof(AUTHS) - 1, &c), 0);
	EXPECT_STREQ(c.username, "user");
	EXPECT_STREQ(c.password, "pass");
	EXPECT_EQ(auth_file(ahead, sizeof(ahead) - 1, &c), -EINVAL);
	EXPECT_STREQ(c.username, "");
	EXPECT_EQ(auth_file(behind, sizeof(behind) - 1, &c), -EINVAL);
	EXPECT_STREQ(c.username, "");
}

static const struct unit_case cases[] = {
	UNIT_CASE(manifest_layer_is_a_whole_descriptor),
	UNIT_CASE(digest_names_a_blob_as_a_registry_does),
	UNIT_CASE(auth_file_with_a_nul_is_refused),
	{}
};

UNIT_SUITE(bpm_oci, cases);
