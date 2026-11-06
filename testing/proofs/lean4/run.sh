#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# testing/proofs/lean4/run.sh -- check the Lean 4 proofs of the models of
# shirudo's algorithms in this directory. A proof left unfinished with
# sorry, or any other warning, fails the build.
#
#   testing/proofs/lean4/run.sh

set -eu

cd "$(dirname "$0")"
command -v lake >/dev/null 2>&1 || {
	printf 'proofs: lake not found; install elan and run elan toolchain install %s\n' \
		"$(cat lean-toolchain)" >&2
	exit 2
}
lake build
