/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * CMS/PKCS#7 signer. Linked only into shirudo-build.
 */

#ifndef SHIRUDO_SIGN_H
#define SHIRUDO_SIGN_H

#include <stddef.h>
#include <stdint.h>

#define SHIRUDO_SHA256_LEN	32

/*
 * The signature and payload_sha256 both span insns || data, the bytes
 * the kernel reconstructs at BPF_PROG_LOAD; data may be empty.
 * signature_sz comes back as the actual size.
 */
struct shirudo_sign_io {
	const void	*insns;
	size_t		insns_sz;

	const void	*data;
	size_t		data_sz;

	void		*signature;
	size_t		signature_sz;

	uint8_t		payload_sha256[SHIRUDO_SHA256_LEN];
};

/*
 * -ENOSPC when the signature buffer is too small, -EINVAL on a parse
 * failure, -EIO on an OpenSSL failure.
 */
int shirudo_cms_sign(struct shirudo_sign_io *io,
		     const char *pkey_path, const char *cert_path);

#endif /* SHIRUDO_SIGN_H */
