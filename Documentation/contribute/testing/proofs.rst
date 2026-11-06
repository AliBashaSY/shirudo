..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: How-to
:audience: shirudo contributors

Prove code with CBMC
====================

A test checks the inputs someone thought of, and a fuzz target the ones it
happens to find. A proof under :src:`testing/proofs/cbmc/` checks every input
within its bounds: `CBMC <https://www.cprover.org/cbmc/>`_ turns shirudo's
C and a harness into a formula and asks a SAT solver for an input that
breaks a property. When none exists, the property holds for all of them.

Each proof states two kinds of property. CBMC adds the first to every
proof: no read or write outside an object, no null or dangling pointer,
no signed overflow, no shift past the width of a type, no division by
zero, no conversion that changes a value, and no loop that runs longer
than the harness allows. The harness states the second: what the code
must do, such as that a parser accepts exactly the well-formed inputs and
that every offset it returns lies inside the input.

A proof covers every input up to the bound its harness names. Where the
format itself bounds what the code reads, as a fixed-size header does, the
proof covers every input of any size; elsewhere it covers every input up
to the stated length, and :doc:`proof-coverage` says which is which.

Before you begin
----------------

- CBMC 5.95 or newer, packaged as ``cbmc`` by Debian and Ubuntu.
- Memory: most proofs need little, but a large one can take several
  gigabytes.

Run the proofs
--------------

1. Run all of them from the checkout:

   .. code-block:: console

      $ make proofs

   Each line names a proof, the number of checks CBMC made and the time
   it took. ``proofs: <n> hold`` ends a run in which all of them held.

2. Run some of them by name:

   .. code-block:: console

      $ make proofs PROOFS="string-number upatch-header"

3. Read why one failed:

   .. code-block:: console

      $ testing/proofs/cbmc/run.sh -v string-number

   ``-v`` prints CBMC's whole output, with the trace of each failed
   check: the input that breaks it and the steps that lead there.

Write a proof
-------------

1. Write ``testing/proofs/cbmc/<name>.c``. It includes ``proof.h`` and the
   source file it proves, so that it reaches static functions too, and
   defines ``harness()``, which CBMC runs with every input at once.
   ``any_bytes(n)`` gives ``n`` bytes of any contents, and a variable
   left uninitialized takes any value; ``__CPROVER_assume()`` narrows the
   inputs to the bound, and ``__CPROVER_assert()`` states each property
   with a message that says what holds.

2. State what the code must do from its contract, not from its code: an
   accepted string is a number of the right digits, an accepted file has
   every section inside it. A check that restates the implementation
   proves nothing about it.

3. Add the proof to :src:`testing/proofs/cbmc/proofs.list` with its CBMC
   options:

   - ``--unwind <n>`` for loops, ``--unwindset <loop>:<n>`` for one loop,
     with ``goto-instrument --show-loops`` naming them. The unwinding
     assertions ``run.sh`` adds fail when an input needs more iterations,
     so a bound too low fails the proof rather than weakening it.
   - ``LEN=<a>-<b>`` runs the proof once for each length, with ``LEN``
     defined: CBMC takes far longer over an input whose length it has to
     keep symbolic.
   - ``--object-bits <n>`` when CBMC reports too many addressed objects.

4. Check that the proof can fail: break the code it proves, for instance
   a loop that reads one byte too many, run it and see it fail, then
   restore the code.

5. Add a row to :doc:`proof-coverage`.

A function without a body in the proof, a libc one CBMC does not model or
a system call, returns any value, and ``run.sh`` fails a proof that calls
one. Give the function a model in ``proof.h``, or keep the proof away from
the code that calls it.

Limits
------

A proof is about the code CBMC sees: the C of the files the harness
includes, with ``-funsigned-char`` as the build sets it, against a model
of the libc functions it calls. It says nothing about the kernel, the
compiler's code generation, time, or other threads, and it holds only
within its bounds.

Related
-------

- :doc:`proof-coverage`
- :doc:`testing`
- :doc:`test-coverage`
