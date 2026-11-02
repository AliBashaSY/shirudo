// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Authors of shirudo
//
// Count an array's elements with ARRAY_SIZE(), which refuses a pointer.

virtual patch
virtual report

@r@
expression E;
position p;
@@
(
  sizeof(E)@p / sizeof(E[...])
|
  sizeof(E)@p / sizeof(*E)
)

@script:python@
p << r.p;
@@
if p[0].current_element == "ARRAY_SIZE":
    cocci.include_match(False)

@script:python depends on report@
p << r.p;
@@
coccilib.report.print_report(p[0], "use ARRAY_SIZE()")

@depends on patch@
expression r.E;
position r.p;
@@
(
- sizeof(E)@p / sizeof(E[...])
+ ARRAY_SIZE(E)
|
- sizeof(E)@p / sizeof(*E)
+ ARRAY_SIZE(E)
)
