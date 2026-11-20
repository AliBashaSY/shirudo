// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Authors of shirudo
//
// The address of a local, stored through a pointer the caller passed in,
// stays in the caller's object when the function returns: the object then
// points into a dead frame, which whoever reads the member next takes for
// what it was told. Hand a callback a context of the function's own
// instead, or clear the member on every path out.

virtual report

@initialize:python@
@@
import re

callers = set()

# A static local outlives the call; its declaration says so ahead of its
# name, on the line of the name or on the ones the declaration began on.
def is_static(d, l):
    with open(d[0].file) as f:
        lines = f.read().splitlines()
    at = int(d[0].line) - 1
    decl = re.split(r"\b%s\b" % re.escape(l), lines[at])[0]
    while at > 0 and not re.search(r"[;{}]\s*$|^\s*$", lines[at - 1]):
        at -= 1
        decl = lines[at] + " " + decl
    return "static" in decl.split()

def escape(fp, d, pos, p, fld, l):
    if (fp[0].file, fp[0].line, p) in callers and not is_static(d, l):
        coccilib.report.print_report(pos[0], "%s->%s keeps the address of the local %s past the return" % (p, fld, l))

// A pointer that came in as a parameter, or was taken from one.
@owner@
attribute name SEC;
identifier f, p;
type T;
position fp;
@@
  f@fp(..., T *p, ...) { ... }

@script:python@
fp << owner.fp;
p << owner.p;
@@
callers.add((fp[0].file, fp[0].line, p))

@alias@
identifier f, p, q;
type T, T1;
position fp;
@@
  f@fp(..., T1 q, ...)
  {
  ... when any
(
  T *p = q;
|
  T *p = (T *)q;
)
  ... when any
  }

@script:python@
fp << alias.fp;
p << alias.p;
@@
callers.add((fp[0].file, fp[0].line, p))

@array exists@
identifier f, p, fld, l;
type T;
expression E;
position fp, d, pos;
@@
  f@fp(...)
  {
  ... when any
(
  T l@d[...];
|
  T l@d[...] = ...;
)
  ... when any
(
  p->fld@pos = l;
|
  p->fld@pos = &l[...];
)
  ... when != p->fld = E
  }

@script:python depends on report@
fp << array.fp;
d << array.d;
pos << array.pos;
p << array.p;
fld << array.fld;
l << array.l;
@@
escape(fp, d, pos, p, fld, l)

@scalar exists@
identifier f, p, fld, l;
type T;
expression E;
position fp, d, pos;
@@
  f@fp(...)
  {
  ... when any
(
  T l@d;
|
  T l@d = ...;
)
  ... when any
  p->fld@pos = &l;
  ... when != p->fld = E
  }

@script:python depends on report@
fp << scalar.fp;
d << scalar.d;
pos << scalar.pos;
p << scalar.p;
fld << scalar.fld;
l << scalar.l;
@@
escape(fp, d, pos, p, fld, l)
