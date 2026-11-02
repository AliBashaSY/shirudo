// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Authors of shirudo
//
// A condition that tests one term twice, or an if whose branches do the
// same, is a copy left in place of what was meant.

virtual report

@r@
expression E;
position p;
@@
(
  E &&@p E
|
  E ||@p E
)

@script:python depends on report@
p << r.p;
@@
coccilib.report.print_report(p[0], "the same term tested twice")

@b@
expression E;
statement S;
position p;
@@
  if (E@p) S else S

@script:python depends on report@
p << b.p;
@@
coccilib.report.print_report(p[0], "both branches of the if do the same")
