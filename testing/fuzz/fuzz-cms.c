// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The certificates and detached CMS signatures that come with indexes,
 * tree publications, packages and keys: a 16-bit little-endian length, a
 * DER certificate of that length and a signature over the certificate.
 */

#include "crypto.h"
#include "fuzz.h"
#include "keys.h"
#include "malloc.h"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct shirudo_cert_pubkey pub;
	struct shirudo_cert cert = {};
	size_t der_len, sig_len, n;
	const __u8 *der, *sig;
	__u8 sha[32], fp[32];
	__u8 *raw;

	if (size < 2)
		return 0;
	der_len = data[0] | (size_t)data[1] << 8;
	if (der_len > size - 2)
		return 0;
	der = data + 2;
	sig = der + der_len;
	sig_len = size - 2 - der_len;

	shirudo_keys_digest_der(der, der_len, sha, fp);
	if (!shirudo_cert_read_der(der, der_len, &cert)) {
		shirudo_cert_pubkey(&cert, &pub);
		shirudo_verify_detached(der, der_len, sig, sig_len, &cert);
		shirudo_cms_signed_by(sig, sig_len, &cert);
		shirudo_cert_free(&cert);
	}

	raw = xmalloc(sig_len + 1);
	if (raw) {
		shirudo_base64_decode((const char *)sig, sig_len, raw,
				      sig_len + 1, &n);
		xfree(raw, sig_len + 1);
	}
	return 0;
}
