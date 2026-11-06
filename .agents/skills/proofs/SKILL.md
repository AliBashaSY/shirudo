---
name: proofs
description: Run, write and debug shirudo's proofs, CBMC under testing/proofs/cbmc, which checks its parsers and emitters for every input within a bound, and Lean 4 under testing/proofs/lean4, which proves models of its algorithms for every input. Covers make proofs, the CBMC harness and proofs.list conventions, writing a Lean model and linking it to the C, choosing properties and bounds, keeping a proof fast and within memory, and telling a real bug from a harness error. Use when changing code a proof covers, when adding a parser or emitter, when a proof fails, or when asked to prove code correct.
---

# Proofs

`Documentation/contribute/testing/proofs.rst` is the how-to, and
`Documentation/contribute/testing/proof-coverage.rst` lists every proof
and what no proof covers yet. A change to code a proof covers runs that
proof, and a change to a proof or to what it covers updates the coverage
page in the same commit.

## Run

```sh
make proofs                                  # CBMC and Lean 4
make proofs-cbmc PROOFS="string-number hex"  # some CBMC proofs
testing/proofs/cbmc/run.sh -v <proof>        # whole output and traces
make proofs-lean4                            # the Lean 4 proofs
```

Cap the memory of a long run, `ulimit -v 24000000` in the shell that
starts it: a proof that grows past a few gigabytes is one to reshape, not
to wait for.

## Write

- `testing/proofs/cbmc/<name>.c` includes `"proof.h"` and the `.c` it
  proves, and defines `harness()`. Inputs come from `any_bytes(n)` and from
  variables left uninitialized; `__CPROVER_assume()` bounds them and
  `__CPROVER_assert(cond, "what holds")` states each property.
- State the contract, not the implementation: which inputs are accepted,
  that every offset or pointer returned lies inside the input, that a
  refusal writes nothing, that output reads back. Proving the code equal
  to a reference that computes the same arithmetic is hard for the SAT
  solver and proves little more: an exact value check belongs on short
  inputs only.
- Prefer a harness that holds for inputs of every size: when the code
  reads a fixed-size part, give it exactly that many bytes and leave the
  size it is told symbolic, as `upatch-header.c` does.
- Then run it on broken code: make a loop read one byte too many or drop
  a check, see the proof fail, and restore the file.

## Keep it fast

- A length CBMC keeps symbolic makes every access to the buffer costly.
  Run the proof once per length instead: `LEN=0-6` or `LEN=16,64` in
  `proofs.list`, with the harness using `LEN`.
- A global `--unwind` unrolls every loop that far, including those whose
  count comes from the data. Give each loop its bound with
  `--unwindset <function>.<n>:<bound>` (`goto-cc --function harness ... -o
  x.goto; goto-instrument --show-loops x.goto` names them) and keep the
  global one small; the unwinding assertions fail the proof if an input
  needs more, so a tight bound is safe.
- A loop the input cannot reach at the harness's sizes takes bound 0, as
  the policy labels do in data too short to hold one.
- `too many addressed objects` asks for `--object-bits 12`.
- A proof over a few hundred symbolic bytes with long string scans can
  take minutes: split it, proving the parts the caller composes, as the
  `.upatch` header, sections and decoders are proven apart.

## Lean 4

CBMC proves the C within a bound; Lean proves a model for every input.
Use Lean where a bounded proof is not enough: an algorithm whose
correctness needs every length, such as `Number.lean`'s exact value of a
digit string, or a protocol property, such as `Chain.lean`'s register
determining its events.

- A model goes in `testing/proofs/lean4/Shirudo/<Name>.lean`, imported
  from `Shirudo.lean`, and follows the C step by step, with the values the
  C can hold (2^64 for a `__u64`).
- What the C cannot show, such as a hash being collision resistant, is
  an explicit hypothesis of the theorem, never an axiom.
- Core Lean only, no Mathlib: `omega`, `simp`, `decide` and induction
  carry these proofs. A lemma Mathlib has, such as
  `List.map_injective_iff`, is proved in the file.
- The lakefile makes every warning an error, so a `sorry`, a deprecated
  name or an unused `simp` argument fails the build.
- Link the model to the C with a CBMC proof where one is cheap, as
  `string-number-model.c` checks `parse_u64()` against `Number.lean` up
  to 4 bytes; the coverage page says which models follow the C by review.
- The toolchain is the one `lean-toolchain` names; elan installs it.

## When it fails

- `no body for function <f>`: CBMC lets that call return anything, so
  `run.sh` fails the proof. Add a model of `<f>` to `proof.h` or keep the
  proof away from it.
- A failed `__CPROVER_assert` or bounds check comes with a trace under
  `-v`. Rebuild the input it names as a unit test or a small C program
  before calling it a bug: a harness that assumed too little or stated
  the property wrongly fails too.
- `run.sh` passes `--unsigned-char`, as the build passes
  `-funsigned-char`; a conversion failure on `char` elsewhere means the
  proof is not built as the code is.

A real bug is fixed in a commit of its own, with the proof that found it
or a unit test, and the proof commit follows.
