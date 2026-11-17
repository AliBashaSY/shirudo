---
name: coding
description: Write or change shirudo's C code, userspace or BPF, in the tree's style. Covers the kernel coding style it follows, where a comment belongs and where it is clutter, the helpers in util/ and include/shirudo/ to use instead of libc or open code, moving code two places need into util/, memory, secrets, JSON and logging, the commit message that carries the change, with a chart where one explains it faster, and the checks a change passes. Use whenever writing, changing or reviewing C in shirudo.
---

# Coding

`Documentation/contribute/code/coding-style.rst` is the kernel's coding
style, and the tree follows it in userspace as well as in BPF: tabs
eight wide, 80 columns where it reads well and never past 100, K&R
braces, short functions that do one thing. AGENTS.md lists the
project's conventions in short; this skill gives them with their
reasons. Read the code around a change first and write like it.

## Write code that explains itself

Write self-documenting code: a reader gets what it does and why from
the names and the shape of the code, without a comment.

- Name a function for what it does and a variable for what it holds:
  `payload_len`, not `n2`; `upatch_name_valid()`, not `check()`; a
  boolean for what is true, `whole`, `found`. A bare number becomes a
  named constant or a `sizeof`.
- A step that would need a comment becomes a small function whose name
  says it, and a condition that needs reading twice becomes a helper or
  a well-named local.
- One function does one job and fits on a screen; errors return early
  instead of nesting the rest.
- The plain construct beats the clever one. Where the clever one is
  needed for correctness or speed, that need is the why worth a comment.
- A comment that restates the code means the code should change: rename
  or split until the comment says nothing new, then drop it.

Instead of

```c
/* Leave out the per-CPU maps. */
if (type == 5 || type == 6 || type == 10)
	continue;
```

write

```c
if (upatch_map_is_percpu(type))
	continue;
```

## Comments

Code says what it does; a comment is for what it cannot say.

- A `.c` file starts with `// SPDX-License-Identifier: BSD-2-Clause`, a
  header with `/* SPDX-License-Identifier: BSD-2-Clause */`, followed by
  the copyright, `Copyright (c) 2026 Authors of shirudo`, and at most
  five lines on what the file holds. Nothing more at the top.
- In a function, almost never. Write one only for the why a reader
  cannot get from the code: an invariant kept elsewhere, a locking rule
  or an order two steps must keep, a kernel or library quirk ("zlib
  1.3.2 reads past the end of a member"), the reason the obvious way is
  wrong here. Before the code it explains, as short as it can be
  without cutting the reasoning: usually a sentence or two, and what a
  lock or a race needs to be understood, however many that takes.
- Never what the code does, what a name already says, or what a message
  prints; never history or narration of the change ("added for",
  "was", "used by", "fix for", "new helper to"), which goes in the
  commit message; never code commented out.
- Drop a comment that carries nothing only in the lines the change adds
  or touches. Leave the comments of code the change does not touch
  alone, even where you would write them shorter.
- A function's contract goes above its declaration in the header, and
  only when the prototype does not tell it: what it returns on which
  failure, what it owns or frees, what it refuses.
- Block comments open with `/*` and close with `*/` on lines of their
  own, every line between starting with ` *`; `//` is for SPDX alone.

Clutter, to leave out:

```c
/* Check the length. */
if (len > max)
	return -E2BIG;
```

Worth writing:

```c
/*
 * The header is not signed: .rodata is where the signed loader takes
 * it from, and a header naming another offset would aim the rewrite at
 * other bytes.
 */
```

## Use util/, and grow it

Never write the same logic twice. Before writing a parser, a check, a
path, a copy loop or a record format, look for one that already does it,
in `util/`, `include/shirudo/` and the builtins alike: `git grep -n
'<word>'`. A variant that differs only in a length check, a bound or a
message is still a copy: make one helper, with a parameter or with the
extra check left to the caller, and move one found in a builtin into
`util/` for both. The basics:

| Need | Use |
|---|---|
| Parse a number | `parse_u64()`, `parse_s64()`, `parse_x64()`, `digits()` (`<shirudo/string.h>`) |
| Copy or format a string | `strscpy()`, `strsprintf()`, `vstrsprintf()`, and check `-E2BIG` |
| An upatch's pin directory or a pin in it | `upatch_pin_path()` (`util/paths.h`) |
| A file name's extension | `name_has_suffix()` (`<shirudo/string.h>`), `upatch_select()` for a scandir of upatches |
| Text from outside on a terminal | `text_sanitize()`; UTF-8 with `utf8_width()` |
| Sizes and lengths | `check_add_overflow()` and kin, `size_add()`, `size_mul()`, `struct_size()` (`<shirudo/overflow.h>`) |
| Smaller or larger | `min()`, `max()` (`<shirudo/misc.h>`) |
| Memory | `xmalloc()`, `xcalloc()`, `xreallocarray()`, `xstrndup()`, `xasprintf()`, `xfree(p, size)` (`util/malloc.h`) |
| Secrets | `memzero_explicit()`, `memneq_explicit()` |
| Files | `read_file()`, `read_fd()`, `write_complete()`, `append_line()`, `lines_len()`, `open_beneath()`, `lock_exclusive()` (`util/io.h`) |
| Sealed state and paths root names | `seal_open()`, `seal_lock()`, `seal_append_line()`, `seal_write_file()`; `seal_output_create()` and `seal_output_commit()` for a report or export (`util/seal.h`) |
| Bytes on the wire | `get_le32()`, `put_be32()`, `le32_to_cpu()` and kin (`<shirudo/byteorder.h>`), `hex_encode()`, `hex_decode()`, `hex_decode_max()`, `sha256_digest_decode()` (`util/hex.h`) |
| JSON | `util/json.h`: `json_parse()` with a schema, `json_walk()`, `struct json_emit` |
| Messages | `pr_err()`, `pr_warn()`, `pr_info()`, `pr_audit()` (`util/log.h`) |

Code that a second place needs moves into `util/<m>.c`, declared in
`util/<m>.h`, or in `include/shirudo/` for the project's basics, with
cases in `testing/unit/<m>.c`, and the copies it replaces go in the same
commit. Two static copies of one helper in two builtins are a bug in
waiting: one gets fixed and the other does not. A util function takes a
length rather than trusting a terminator in outside data, returns 0 or
a negative errno, and leaves the message to its caller. Operator-only
code, signing and the package builder, stays out of the core binary.

## Conventions

- Plain C; zero-initialise with `= {}`.
- Integers are `__u8` to `__u64` and `__s8` to `__s64`; `__u64` prints
  with `%llu`, and `ptr_to_u64()` gives the `__u64` the bpf() ABI takes.
  A library interface that takes another type keeps it, and BPF spells
  hook arguments as the kernel's prototypes do. `check-style.sh` refuses
  `uint32_t` and its kin, and in userspace `unsigned int`, `long` and
  the like. System headers that use `uint8_t` and kin go above the first
  shirudo header, which poisons them.
- `strtoul()`, `atoi()`, `strcpy()`, `sprintf()`, `snprintf()`, the libc
  allocators and `explicit_bzero()` are poisoned; the table above has
  what replaces each, and a missing one is wrapped in `util/`, never
  given an exception.
- A `strsprintf()` that does not fit leaves the buffer empty: a path or
  value is refused, a helper formatting display text returns the error,
  and status shows `"?"`. Constant text is copied, not formatted.
- `xstrdup()` measures with `strlen()`, so outside input gets
  `xstrndup(s, bound)`. `xfree(p, size)` and `xstrfree()` scrub before
  they free.
- `min_t()` and `max_t()` cast and can cut a value, so they are for
  only what `min()` and `max()` refuse; a constant compared with an
  unsigned value takes a `U` suffix.
- `ret` holds a return code, `err` an errno kept beside the value a
  function returns. No `(void)` casts: a result that must be checked
  comes from a `__must_check` function, and every call checks it.
- Internal names carry no `shirudo_` prefix; it stays only where a name
  leaves the code: include guards, BPF object and pin names, environment
  variables, a clash with libc.
- Locals in reverse-Christmas-tree order, longest line first, except
  where an initializer needs an earlier local.
- Never two blank lines in a row. Removing code often brings the blank
  lines around it together: drop one of them. checkpatch checks only
  the lines a patch adds, so it does not see a pair a deletion makes.
- A frame past 128 KiB fails the build: tables go on the heap, and a
  helper with a big buffer is `noinline_for_stack`.
- Scrub a key, a seed, a credential or a buffer that held one with
  `memzero_explicit()` on every path out, error paths included, and
  compare it with `memneq_explicit()`. Mark a struct member that holds
  one `__sensitive`: a check then wants every local of that struct
  scrubbed the same way, member filled by a parser and all.
- All JSON goes through `util/json.c`; text from outside is written
  with its lossy writers. All logging goes through `pr_*()` to syslog;
  shirudo writes no log file of its own.
- Fields of a little- or big-endian format are read and written with
  `le32_to_cpu()`, `get_le32()` and their kin; sparse refuses the rest.
- Userspace is BSD-2-Clause. Every file a BPF object compiles, and each
  Lua decoder, is `GPL-2.0 OR BSD-2-Clause`, with
  `char _license[] SEC("license") = "Dual BSD/GPL";`. No emojis.
- BPF code is CO-RE, built for `-mcpu=v4`; upatch code follows the
  `upatch` and `bpf-concurrency` skills.

## Commit messages

`Documentation/contribute/code/contributing.rst` sets the form: the
subject `area: imperative summary`, specific ("seal: refuse the
runtime to a task that shares its file system context", not "seal:
fixes") and at most 75 characters; the body wrapped at 75 columns, the
problem and its impact before the change. Write it for a reviewer who
reads it once:

- Short, plain sentences, one idea each. No filler and no long
  background; drop any sentence the reviewer does not need to
  understand the problem or the fix.
- Plain paragraphs for a simple bug. `[BUG]`, `[CAUSE]` and `[FIX]`
  headings only where the symptom, the cause and the fix each take
  several paragraphs.
- Quote what a program printed, an error, a log line or a call trace,
  as it printed it: quoted text has no column limit and is never
  folded.
- Read `git log -- <the files>` for the recent messages of the area
  before writing one.
- Where a chart explains the problem faster than prose, draw one.

### Charts

An ASCII chart earns its place for:

- a race: show the window, side by side, rather than describe it;
- an error path seen from several callers, where one cleans up what
  another already did;
- a call chain across layers where the bug shows on one path only;
- the order of steps, as in a load's pins, archive and extend, or
  `init`'s phases.

Function names only, no file paths or line numbers, and only the
functions that matter. A state or an outcome goes in plain words under
the function name. A race takes one column per racing context: a
command, root, a CPU, a kworker.

A race, a load that root stops between its pins and its extend:

```
 shirudo upatch load                root
 upatch_load()
 |-upatch_apply()
 | \-attach_and_pin_progs()
 |   The links are attached
 |                                  kill -9 <pid>
 |                                  The load ends here
 \-upatch_chain_event_locked()
   Never runs: pins without a record in the register.
```

A call chain where the bug shows on one path, an exec that fails past
its point of no return:

```
 load_elf_binary()
 |-begin_new_exec()
 | \-security_bprm_committed_creds()
 |   The task now holds runtime authority
 |
 \-create_elf_tables()
   Fails: SIGSEGV, and the core dump runs as the task
   \-coredump_file()
     Unlinks the core_pattern path with that authority.
```

## Check

A warning fails the build, so fix it rather than silence it. Build with
GCC and with clang 23 (`make -B` when switching), run the unit suite of
every `util/` file the change touches, adapt and run the proofs under
`testing/proofs/` that reach the code (the `proofs` skill), and run the
`pre-submit` skill's checks before handing the change over: checkpatch
in strict mode, Coccinelle, sparse and both sanitizers find what review
misses.
