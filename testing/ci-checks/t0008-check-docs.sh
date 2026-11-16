#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t0008 -- .github/scripts/check-docs.sh: the whole of each field of a
# threat's fact box and a hook by its section, on a throwaway repository,
# and the tree itself.

cd "$(dirname "$0")/.."
. ./test-lib.sh
command -v git >/dev/null 2>&1 || test_skip_all "git not installed"
ROOT=$(cd .. && pwd)
S=$ROOT/.github/scripts/check-docs.sh
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
R=$W/repo
TM=Documentation/security/threat-model.rst
COV=Documentation/contribute/testing/proof-coverage.rst

# A file with the given lines, each argument one line.
put() {
	f=$R/$1
	shift
	mkdir -p "$(dirname "$f")"
	printf '%s\n' "$@" >"$f"
}

# A tree the check passes: one fact box whose fields continue on a second
# line.
good() {
	rm -rf "$R" && mkdir -p "$R" && git init -q "$R" &&
	put shirudo.c 'int main(void) { return 0; }' &&
	put builtin-upatch.c 'int upatch;' &&
	put tools/shirudo-build/build.c 'int build;' &&
	put tools/gen-image/gen '#!/bin/sh' &&
	put tools/gen-initrd/gen '#!/bin/sh' &&
	put util/tamper.c 'static const struct point points[] = {' \
		'	[0] = { "Good", 1 },' '};' &&
	put bpf/guard.c 'SEC("lsm/file_open")' 'SEC("lsm.s/inode_setattr")' &&
	put bpf/lib/guard.h 'struct guard;' &&
	put testing/functional/build/t1001-one.sh '#!/bin/sh' &&
	put testing/proofs/cbmc/proofs.list 'first --unwind 1' 'second --unwind 1' &&
	put testing/proofs/cbmc/first.c '#include "proof.h"' &&
	put testing/proofs/cbmc/second.c '#include "proof.h"' &&
	put testing/proofs/lean4/Shirudo/Model.lean 'theorem model : True := trivial' &&
	put Documentation/security/tamper-counters.rst '``Good``' &&
	put "$TM" ':Enforced at: ``lsm/file_open``,' \
		'   ``lsm.s/inode_setattr``' \
		':Counters: none for a first attempt,' \
		'   ``Good``' \
		':Tests: ``t1001``' \
		':Proofs: ``first``,' \
		'   ``second``' &&
	put "$COV" '   * - ``first``' '   * - ``second``' \
		'   * - ``Shirudo.Model``'
}

check() {
	(cd "$R" && git add -A && sh "$S") >"$W/out" 2>"$W/err"
}

# The check fails and says so.
refuses() {
	! check && grep -qF "$1" "$W/err"
}

test_expect_success 'a tree whose fields continue on a second line passes' '
	good && check
'

test_expect_success 'a hook on a continuation line is checked, by its section' '
	good && sed -i "s|lsm.s/inode_setattr|lsm.s/inode_setattrx|" "$R/$TM" &&
	refuses "no BPF program has the section lsm.s/inode_setattrx" &&
	good && sed -i "s|lsm.s/inode_setattr|lsm/inode_setattr|" "$R/$TM" &&
	refuses "no BPF program has the section lsm/inode_setattr"
'

test_expect_success 'a counter and a proof on a continuation line are checked' '
	good && sed -i "s|\`\`Good\`\`|\`\`Goodx\`\`|" "$R/$TM" &&
	refuses "no tamper counter is named Goodx" &&
	good && sed -i "s|\`\`second\`\`|\`\`secondx\`\`|" "$R/$TM" &&
	refuses "no proof is named secondx"
'

test_expect_success 'the tree itself passes' '
	(cd "$ROOT" && sh "$S") >"$W/out" 2>"$W/err"
'

test_done
