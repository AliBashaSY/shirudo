#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo

set -eu
cd "$(dirname "$0")"
: "${SHIRUDO_BIN:?runtime command is required}"
: "${SHIRUDO_TEST_FIXTURES_DIR:?signed upatch fixtures are required}"
: "${SHIRUDO_TEST_KEY_FIXTURES:?signed image fixtures are required}"
: "${SHIRUDO_TEST_RESULTS:=/tmp/shirudo-verity-results}"
: "${SHIRUDO_TEST_SUITE_FAILURES_FILE:=}"
[ "$(id -u)" = 0 ] && [ -x "$SHIRUDO_BIN" ] &&
	[ -e /run/shirudo/bpffs/task_authority ] &&
	mountpoint -q /run/shirudo/runtime && mountpoint -q /run/shirudo/config || {
	printf 'image suite requires an initialized dm-verity target\n' >&2
	exit 1
}
for name in config-boot-keys config-live-new-key config-a config-b config-bad config-revoke \
	config-tree-stable config-tree-updated config-tree-deleted config-tree-restored \
	config-otel config-jsonl config-jsonl-bad config-interrupted; do
	for suffix in img verity roothash p7s; do
		[ -s "$SHIRUDO_TEST_KEY_FIXTURES/$name.$suffix" ] &&
			[ -s "/var/lib/shirudo/images/$name.$suffix" ] || {
			printf 'missing required image fixture: %s.%s\n' "$name" "$suffix" >&2
			exit 1
		}
	done
done
mkdir -p "$SHIRUDO_TEST_RESULTS"
export SHIRUDO_BIN SHIRUDO_TEST_KEY_FIXTURES
export SHIRUDO_TEST_CONFIG_RESULTS="$SHIRUDO_TEST_RESULTS/config"
export SHIRUDO_TEST_SIGNER_RESULTS="$SHIRUDO_TEST_RESULTS/signers"
export SHIRUDO_TEST_INTERRUPTION_RESULTS="$SHIRUDO_TEST_RESULTS/interruption"
export SHIRUDO_TEST_CLI_RESULTS="$SHIRUDO_TEST_RESULTS/cli"
export SHIRUDO_TEST_REVOKE_RESULTS="$SHIRUDO_TEST_RESULTS/revocation"
fail=0
# Boot assertions precede updates; the interruption leaves state pending.
for t in \
	functional/image/t5101-verity-boot-signers.sh \
	functional/tamperproof/t6101-mode-objects.sh \
	functional/upatch/t3101-target-fixtures.sh \
	functional/tpm/t4101-initrd.sh \
	functional/signing/t2101-keys.sh \
	functional/signing/t2102-keys-add.sh \
	functional/signing/t2103-keys-revoke.sh \
	functional/tamperproof/t6102-siggate.sh \
	functional/upatch/t3102-upatch-arena-serializer.sh \
	functional/upatch/t3103-version-gate.sh \
	functional/upatch/t3104-payload-mismatch.sh \
	functional/upatch/t3105-load-unload.sh \
	functional/tpm/t4102-tpm-chain.sh \
	functional/upatch/t3106-upatch-get.sh \
	functional/upatch/t3107-uprobe-multi.sh \
	functional/bpm/t7101-bpm-tree.sh \
	functional/bpm/t7102-bpm-tree-sequential.sh \
	functional/bpm/t7103-bpm-tree-delete.sh \
	functional/otel/t8101-map-export-all.sh \
	functional/otel/t8102-otel-backend.sh \
	functional/otel/t8103-otel-serialized-metric.sh \
	functional/tpm/t4103-nv-writer.sh \
	functional/tamperproof/t6103-lsm-deny.sh \
	functional/tamperproof/t6104-prog-load-keyring-gate.sh \
	functional/tamperproof/t6105-mount-lockdown.sh \
	functional/tamperproof/t6106-pin-rename.sh \
	functional/tamperproof/t6107-bpf-cmd-deny.sh \
	functional/tamperproof/t6108-tamper-counters.sh \
	functional/tamperproof/t6109-map-lockdown.sh \
	functional/tamperproof/t6110-pr-set-mm-identity.sh \
	functional/tamperproof/t6111-seal-scratch.sh \
	functional/tamperproof/t6112-key-permission.sh \
	functional/tamperproof/t6113-verity-authority.sh \
	functional/tamperproof/t6114-output-paths.sh \
	functional/tamperproof/t6115-archive-restore.sh \
	functional/tamperproof/t6116-attach-scope.sh \
	functional/tamperproof/t6117-signed-metadata.sh \
	functional/tamperproof/t6118-foreign-entries.sh \
	functional/tamperproof/t6119-perf-attach.sh \
	functional/tamperproof/t6120-umount-above.sh \
	functional/tamperproof/t6121-bpffs-entries.sh \
	functional/tamperproof/t6122-persist-undo.sh \
	functional/tamperproof/t6123-foreign-state-root.sh \
	functional/tamperproof/t6124-overlay-copy-up.sh \
	functional/tamperproof/t6125-tpm-identity.sh \
	functional/tamperproof/t6126-kexec.sh \
	functional/tamperproof/t6127-proc-substitution.sh \
	functional/tamperproof/t6128-register-lock.sh \
	functional/tamperproof/t6129-keyring-seal.sh \
	functional/tamperproof/t6130-verity-devices.sh \
	functional/tamperproof/t6131-verity-process.sh \
	functional/tamperproof/t6132-verity-hidden.sh \
	functional/image/t5102-verity-config.sh \
	functional/image/t5103-verity-cli.sh \
	functional/image/t5104-verity-revocation.sh \
	functional/image/t5105-verity-interruption.sh
do
	log="$SHIRUDO_TEST_RESULTS/$(basename "$t").log"
	rc=0
	sh "$t" >"$log" 2>&1 || rc=$?
	cat "$log"
	if grep -q '^1\.\.0 # SKIP' "$log"; then
		printf 'required image suite was skipped: %s\n' "$t" >&2
		rc=1
	fi
	if [ "$rc" -ne 0 ]; then
		fail=$((fail + 1))
		if [ -n "$SHIRUDO_TEST_SUITE_FAILURES_FILE" ]; then
			printf '%s\n' "$t" >>"$SHIRUDO_TEST_SUITE_FAILURES_FILE"
		fi
	fi
done
[ "$fail" -eq 0 ]
