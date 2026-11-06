---
name: upatch
description: Write, build, sign, inspect and package a shirudo upatch. Covers the BPF C program, `shirudo build` for a .upatch and its unload directive, the Beefile and `shirudo bpm build` for a .bee package, `bpm info` to inspect one, `bpm push` and `bpm tree append` to publish, and loading it on a test target. Use when asked to create, change or ship an upatch or a bpm package, or to reproduce a failed upatch build or load.
---

# Upatch

`Documentation/upatches/` is the reference: `learn/concepts.rst`, then
under `write/` the attach kinds, maps, arenas and serializers, decoders,
pitfalls and memory consistency, under `publish/` the Beefile and
publishing, and under `test/` testing and debugging a failed load.
`Documentation/start/guides/first-upatch.rst` and `publish-package.rst`
walk through every command below.

## Write the program

- Plain C against `vmlinux.h` with CO-RE: never a hard-coded struct
  offset, which signs fine and is refused by the verifier on another
  kernel (`verifier_rejected`).
- Attach only with the kinds `write/attach-kinds.rst` lists. Refuse an
  operation in an `lsm` program, never in `fmod_ret`
  (`write/pitfalls.rst`).
- `char _license[] SEC("license") = "Dual BSD/GPL";`, and
  `SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause` on every file the
  object compiles and on each Lua decoder.
- Keep accumulated records in one `BPF_MAP_TYPE_ARENA` map with a
  `shirudo_serialize__<map>` program that walks them;
  `upatches/kernel-obs/obs_table.h` is the template.
- Where runs share state across CPUs or interleave on one, follow the
  `bpf-concurrency` skill.
- `shirudo build` compiles with `-mcpu=v4 -fno-jump-tables` itself, so
  the whole v4 instruction set is there: write load-acquire,
  store-release, sign extension and signed division directly.

## Build and sign one upatch

```sh
shirudo genkey --keys-dir <keys>          # once; the operator's key pair
shirudo build <prog>.bpf.c --keys-dir <keys> -o <name>.upatch
shirudo upatch info <name>.upatch         # --json for scripts
shirudo build --unload <name> --keys-dir <keys> -o <name>.unload.upatch \
	--payload-sha256 "$(shirudo upatch info --json <name>.upatch | jq -r .payload_sha256)"
```

- In a checkout, build the helper with `make shirudo-build` and name it
  with `SHIRUDO_BUILD=<checkout>/tools/shirudo-build/shirudo-build`.
- The helper runs the clang it was built for, `SHIRUDO_CLANG_PATH`,
  `/usr/bin/clang` unless set, and that must be clang 23: an older one
  stops at a release store with `Cannot select: ... AtomicStore`.
- It looks for `vmlinux.h` as `./vmlinux.h`, `./vendor/linux/vmlinux.h`
  or the installed one, unless `--vmlinux` names it. Run it from the
  checkout, or link `vendor/` and `bpf/` into the package directory as
  the publishing guide does.

## Build a package

The Beefile (`publish/beefile.rst`) names the package and its upatches,
and optionally generated `.rodata` (`[[upatches.rodata]]`), tree
metadata (`[upatches.tree]`) and extra files. A `README.md` beside it
is what `bpm info` prints.

```sh
shirudo bpm build <dir> --version <semver> --keys-dir <keys>   # <name>-<version>.bee here
shirudo bpm info <name>-<version>.bee
shirudo bpm info upatches <name>-<version>.bee [--all]
shirudo bpm info data <name>-<version>.bee upatches/<upatch>.upatch [--json]
```

The version is SemVer 2.0.0 and nothing else. `bpm info data` decodes
the `.rodata` a rodata entry wrote, the quickest check of a generator.

## Publish

```sh
shirudo bpm push <name>-<version>.bee oci://<registry>/<namespace>/<repo> --keys-dir <keys>
shirudo bpm tree append <name>-<version>.bee oci://<registry>/<repository> [<append-options>]
```

`push` signs the namespace's index along with the package. Pass
`--new-index` for the first package of a namespace and never after.
`BPM_OCI_SCHEME=http` is for a registry on the loopback only. `tree
append` adds a load or delete operation to a signed upatch tree that
targets sync; `Documentation/reference/cli/bpm.rst` lists its options.

## Load it on a target

Only in the test VM (the `testing` skill), with the guest's
`shirudo-runtime`: `upatch load <name>.upatch`, `upatch map`, then
`upatch unload <name>.unload.upatch`; a package with `bpm install` and
`bpm uninstall`. A failed load names a category from
`Documentation/reference/failure-categories.rst`;
`Documentation/upatches/test/debugging.rst` says what each one means and
how to find the cause.
