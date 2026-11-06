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
   * - ``string-suffix``
     - ``name_has_suffix()``
     - Names up to 6 bytes, suffixes up to 3
     - True exactly when the name's last bytes are the suffix and at
       least one byte comes before them
   * - ``hex``
     - ``hex_encode()``, ``hex_decode()``, ``hex_decode_max()``
     - Up to 4 bytes, strings up to 9 characters
     - Lowercase hex that decodes back to the input; a decode takes
       exactly ``2 * n`` hex digits of either case and yields their
       value, and ``hex_decode_max()`` any even count up to ``2 * cap``,
       reading no further than the digit after them
   * - ``io-lines``
     - ``lines_len()``
     - Up to 8 bytes
     - Ends at a newline or is 0, with no newline behind it
   * - ``attest-pcrs``
     - ``attest_parse_pcrs()``, ``attest_parse_pcr_value()``
     - Up to 6 bytes
     - A list is accepted exactly when it is decimal PCRs 0 to 23 apart
       by single commas, into the mask of those PCRs; a value exactly
       after a PCR to 23 and an equals sign
   * - ``attest-name-digest``
     - ``attest_parse_name_digest()``
     - Lengths 64 to 70
     - Accepted exactly when a name of a byte or more runs to the first
       equals sign and 64 hex digits follow, into that name and their
       value
   * - ``paths-bpffs-name``
     - ``upatch_bpffs_name()``, ``upatch_name_from_bpffs()``
     - Names up to 4 bytes, pin names up to 8
     - A name without ``~`` maps to a pin name without a dot and back to
       itself; a pin name without a dot that decodes is the encoding of
       what it decodes to
   * - ``bpm-tree-follows``
     - ``tree_publication_follows()``
     - Every sequence number and generation; strings up to 2 bytes
     - Accepts exactly a publication of the same tree, of a later
       generation unless the same manifest, naming the recorded parent
       when the next generation, with the node's cursor between its base
       and head; without a state, one that starts at 0
   * - ``bpm-upatch-member``
     - ``bpm_upatch_member()``, ``bpm_upatch_pair()``
     - Lengths 0 to 18
     - What install loads and uninstall unloads goes by the suffix alone;
       a pair is terminated, the other half, and pairs back to the name
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
   * - ``upatch-signed-sections``
     - ``parse_signed_sections()``
     - Data of 15, 16, 40 and 64 bytes
     - Sections kept only behind a valid trailer, the directory below
       the trailer, each section inside the data below the directory;
       nothing kept on refusal
   * - ``upatch-signed-name``
     - The ``NAME`` and ``DECODERS`` sections of
       ``parse_signed_sections()``
     - A one-section directory in 288 bytes
     - A kept name is terminated within 256 bytes; a refusal keeps
       neither
   * - ``upatch-decoders``
     - ``parse_decoders()``, ``upatch_decoder_view()``,
       ``upatch_has_decoders()``
     - Sections of every length declaring up to 2 decoders
     - Names terminated, each source non-empty and inside the section
       behind the entries, every kind set, nothing kept on refusal,
       ``-ENOENT`` past the last decoder
   * - ``upatch-policy-labels``
     - ``parse_policy_labels()``
     - Sections of 0, 1 and 2 labels and the lengths around them
     - An accepted section fills its length exactly, with its magic and
       version, two distinct map indexes below the map count, one bit
       per label and a terminated name and absolute path; a refusal
       writes nothing
   * - ``upatch-name``
     - ``upatch_name_valid()``, ``upatch_select()``,
       ``upatch_prog_name()``
     - Names up to 24 bytes, archive names up to 31
     - Valid exactly for ``[A-Za-z0-9._-]+`` not starting with a dot;
       an archive entry exactly for a name and ``.upatch``; a program
       name zero-padded, of the header's bytes or ``_`` in their place
   * - ``upatch-prog-specs``
     - ``prog_specs_parse()``, ``prog_specs_free()``
     - Tables of 3, 15, 16 and 176 bytes, as many programs as fit, every
       allocation allowed to fail
     - A refusal leaves nothing allocated and ``*out`` NULL; an accepted
       program has a known role and kind, a terminated target and map,
       and uprobe fields only on a uprobe
   * - ``upatch-prog-uprobes``
     - The uprobe target records of ``prog_specs_parse()``
     - One program and one record in 226 bytes, path and build ID up to
       9 bytes, up to 2 offsets
     - An absolute path of its stated length without a NUL, a target
       without ``,`` or ``|``, a build ID of 1 to 64 bytes, offsets
       non-empty and strictly rising, everything freed
   * - ``upatch-loader``
     - ``loader_lddw()``, ``sys_bpf_call()``, ``map_create_slot()``,
       ``map_update_slot()``, ``loader_store_source()``
     - Loaders of 4, 5, 25 and 49 instructions
     - Reads only the loader, and only instructions before the call it
       looks back from; reports exactly what it matched; every slot
       below the table's size
   * - ``upatch-rodata-btf``
     - ``loader_btf()``
     - Loaders of 4, 10 and 15 instructions and 24 bytes of data
     - The BTF it locates lies inside the data
   * - ``upatch-rodata``
     - ``find_rodata_data()``, ``map_name_rodata()``
     - Any loader of 6 or 8 instructions; at 24, the shape the loader
       generator emits with every other field free; 48 bytes of data
     - The value it locates lies inside the data, with the size and type
       its section declares
   * - ``upatch-btf``
     - ``btf_name_printable()``, ``btf_decl_printable()``,
       ``btf_unwrap()``
     - BTF of 4 types, names up to 6 bytes, libbpf modelled
     - A printed name is printable ASCII, an accepted declaration chain
       has no loop, an unwrapped type is 0 or has no qualifier
   * - ``upatch-btf-int``
     - ``btf_integer()``
     - Every encoding and every value of up to 8 bytes
     - The bits stay inside the integer's bytes; the value is zero- or
       sign-extended from them

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
does: the JSON parser and emitter, the package archive, index and
manifest readers, the tree publication and log, the eventlog, the TPM
structures of evidence, the configuration files and images, and the
kernel notes and configuration.

Within the ``.upatch`` reader, the proofs cover its parts but not their
composition: ``upatch_parse()`` and ``upatch_open_memory()`` join the
header, the signed sections, the decoders and the policy labels the
proofs cover one by one, and a proof of the whole file runs out of
memory. A program table of two or more programs, ``find_rodata_data()``
on loaders of other shapes, ``btf_dumpable()``, the ``.rodata`` printers
and the libbpf calls behind ``btf_rodata_btf()`` have no proof.
