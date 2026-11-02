// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Authors of shirudo
//
// A length that does not match the string literal it bounds compares
// or copies more or less than the literal.

virtual report

@r@
expression E;
constant char[] S;
constant N;
identifier f = {strncmp, strncasecmp, memcmp, memcpy};
position p;
@@
(
  f@p(E, S, N)
|
  f@p(S, E, N)
)

@script:python depends on report@
p << r.p;
S << r.S;
N << r.N;
f << r.f;
@@
import ast
try:
    text = ast.literal_eval(S)
    n = int(N, 0)
except Exception:
    text = None
if text is not None and n != len(text.encode()) and \
   not (f in ("memcmp", "memcpy") and n == len(text.encode()) + 1):
    coccilib.report.print_report(p[0], "%s() of %d bytes against a %d-byte literal" % (f, n, len(text.encode())))
