// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Authors of shirudo
//
// sizeof a pointer where the object it points to was meant: what is
// copied, cleared, compared, scrubbed or allocated is 8 bytes.

virtual report

@r@
type T;
idexpression T *P;
identifier f = {memcpy, memmove, memset, memcmp, memzero_explicit,
		memneq_explicit, xfree, xmalloc, read, write, pread, pwrite,
		strscpy, strsprintf};
position p;
@@
(
  f(..., sizeof@p(P), ...)
|
  f(..., sizeof@p(P) * ..., ...)
|
  xcalloc(..., sizeof@p(P))
|
  sizeof@p(sizeof(...))
)

@script:python depends on report@
p << r.p;
@@
coccilib.report.print_report(p[0], "sizeof of a pointer; the object it points to may be meant")
