..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: shirudo contributors

Proof coverage
==============

This page lists the CBMC proofs under :src:`testing/proofs/cbmc/` and the
Lean 4 proofs under :src:`testing/proofs/lean4/`, what each one proves and
for which inputs, and the parsers no proof covers yet. :doc:`proofs`
explains how to run and write them.

CBMC proofs
-----------

Every CBMC proof shows that the code reads and writes nothing outside its
objects and has no undefined arithmetic for the inputs it covers, besides
the properties the table lists. "Every input" means every input of any
length. A bound such as "up to 22 bytes" means every input of that length
or shorter, and "lengths 0 to 6" every input of each of those lengths.

.. list-table::
   :header-rows: 1
   :widths: 22 26 18 34

   * - Proof
     - Code
     - Inputs
     - Properties
   * - ``string-number``
     - ``parse_u64()``, ``parse_x64()``, ``parse_s64()``, ``digits()``
     - Up to 22 bytes, past the 20 digits of the largest value
     - Refuses exactly what is empty or holds a non-digit; a number
       within its bounds and of the sign the string spells; nothing
       written on refusal; ``digits()`` finds the whole run
   * - ``string-number-model``
     - ``parse_u64()``, ``parse_x64()``
     - Up to 4 bytes
     - The value of the Lean model ``Shirudo.Number``, or ``-ERANGE``
       exactly where the model refuses
   * - ``string-utf8``
     - ``utf8_width()``
     - Every input
     - Equals Table 3-7 of the Unicode Standard, the well-formed byte
       sequences, row by row
   * - ``string-sanitize``
     - ``text_sanitize()``
     - Lengths 0 to 6, with and without line breaks
     - Leaves well-formed UTF-8 without a C0, DEL or C1 control but a
       kept line break; changes a byte only to ``?``; keeps every
       printable character
   * - ``string-copy``
     - ``strscpy()``
     - Strings and buffers up to 8 bytes
     - Terminates what it writes; the whole string and its length, or
       what fits and ``-E2BIG``
   * - ``hex``
     - ``hex_encode()``, ``hex_decode()``
     - Up to 4 bytes, strings up to 9 characters
     - Lowercase hex that decodes back to the input; a decode takes
       exactly ``2 * n`` hex digits of either case and yields their value
   * - ``io-lines``
     - ``lines_len()``
     - Up to 8 bytes
     - Ends at a newline or is 0, with no newline behind it
   * - ``upatch-header``
     - ``parse_header()``, ``validate_bounds()`` of the ``.upatch``
       reader
     - Every file: the header is read from a buffer of exactly its size
       for files of any size
     - Reads nothing past the header; an accepted header has its magic,
       version, zero reserved field and a terminated name, a BPF
       payload, a loader and a signature, whole aligned instructions,
       every section inside the file and behind the header, and a
       ``.rodata`` offset inside the data

Lean 4 proofs
-------------

Each proof holds for every input of the model; the last column says how
the model follows the C.

.. list-table::
   :header-rows: 1
   :widths: 22 30 26 22

   * - Model
     - Theorems
     - Assumptions
     - Follows the C
   * - ``Shirudo.Number``, ``parse_base()`` of :src:`util/string.c`
     - ``parse_exact``: for digit strings of every length, the value they
       spell, or ``-ERANGE`` exactly when it passes ``max``
     - ``max`` fits a ``__u64``
     - CBMC, ``string-number-model``, up to 4 bytes
   * - ``Shirudo.Chain``, the register of :src:`util/chain.c`
     - ``register_injective``, ``register_tagged_injective``: equal
       registers come from the same events, tagged the same, in the same
       order; ``register_eq_foldl``: the register folds the events in the
       order they came
     - The hash is injective on (register, event) pairs and never yields
       the initial value; the tagged event encoding is injective
     - Review

Not covered yet
---------------

The fuzz targets in :doc:`test-coverage` cover these parsers; no proof
does: the JSON parser and emitter, the ``.upatch`` signed sections,
decoders and policy labels, the ``.rodata`` view, the package archive,
index and manifest readers, the tree publication and log, the eventlog,
the TPM structures of evidence, the configuration files and images, and
the kernel notes and configuration.
