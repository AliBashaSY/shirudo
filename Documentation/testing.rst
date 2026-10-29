..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

Testing shirudo
===============

shirudo is tested in tiers, each under its own directory of ``testing/``:
unit tests for the functions of ``util/``, functional tests for one command
or mechanism at a time, end-to-end tests for whole delivery paths, tests of
the scripts CI runs, and libFuzzer targets for the parsers of input from
outside the host.  A change comes with a test in the lowest tier that can
show it works.  See `requirements <requirements.rst>`__ for what the test
hosts need and `contributing <contributing.rst>`__ for what to run before
sending a change.

1) The layout
-------------

::

    testing/
        unit/                   unit tests, util/<m>.c by unit/<m>.c
        ci-checks/              the scripts under .github/scripts
        functional/<family>/    one command or mechanism at a time
        end2end/                whole delivery paths on a test VM
        fuzz/                   libFuzzer targets and seed corpora
        fixtures/               BPF programs, decoders and configs the
                                tests build from
        helper/                 C and Python helpers the tests run
        vm/                     the test VM: kernel, bundle, guest
        test-lib.sh             the TAP harness of the shell tests
        integration-lib.sh      shared setup of the target tests
        run-tests.sh            the unit and operator tests
        run-target-tests.sh     one family's target tests, in its VM

Each ``functional/`` family may carry a ``test.plan.md`` that maps the cases
it must cover to the tests that do, and says which are not covered yet.

2) Test numbers
---------------

A shell test is named ``t<F><K><NN>-<what>.sh``, the way git numbers its
test families:

* ``F`` is the family, and with it the directory;
* ``K`` is ``0`` for an operator test, which runs on the build host and
  needs no root, kernel or TPM of its own, and ``1`` for a target test,
  which runs as root in the test VM against an initialised target;
* ``NN`` orders the tests of a family, and is the order they run in.

=== ============================ ==========================================
F   Directory                    What it covers
=== ============================ ==========================================
0   ``ci-checks/``               DCO, SPDX, style, commit range, SBOM
1   ``functional/build/``        dispatch, static target, packaging,
                                 initrd installer, the test VM scripts
2   ``functional/signing/``      keys, signatures, the trusted set
3   ``functional/upatch/``       the container, build metadata, load,
                                 unload, serializers
4   ``functional/tpm/``          the register, eventlog, attestation and
                                 initrd binding
5   ``functional/image/``        runtime and config images, activation,
                                 revocation, interruption
6   ``functional/tamperproof/``  attacks on the LSM, the seal, the
                                 keyrings, bpffs and the TPM path
7   ``functional/bpm/``          packages, repositories, the upatch tree
8   ``functional/otel/``         map export, status, TLS policy
9   ``end2end/``                     the kernel-obs package, delivery paths
=== ============================ ==========================================

A new test takes the next free number of its family and kind; a number is
not reused for a different test.  A file under ``testing/`` that is not a
test does not take a name of this form.

3) Running the tests
--------------------

``make tests`` or ``make -C testing``
    Runs every test, tier by tier: ``unit``, ``ci-checks``, ``functional``
    and ``end2end``.  CI runs the same.

``make -C testing <tier>``
    Runs one tier.  ``unit/<suite>`` runs one unit suite and
    ``functional/<family>`` one family; ``make -C testing help`` lists
    them.  The unit tests and the ``ci-checks/`` tests run on the build
    host.

A functional or end-to-end tier runs its operator tests first, in number
order, then its target tests: each family boots a VM of its own from one
bundle, through the production path, the dm-verity runtime and the signed
config from a dracut initrd.  The tpm family also boots a VM before
``init``, without the production initrd, for the TPM register tests.  The
VMs need root, and ``make -j`` boots several side by side.  A VM that
passed keeps its logs and drops its disks; one that failed is kept under
``build/test-vm/vms/`` for inspection.

``VM=0`` leaves out the tests that boot a VM, for a host without KVM or
root; ``make tests VM=0 SANITIZE=address,undefined`` builds and runs the
rest under ASan and UBSan, as CI does.

``make -C testing fuzz [FUZZ_SECONDS=60]``
    Runs every libFuzzer target on its seed corpus under ASan and UBSan.

``make -C testing vm``
    Boots an interactive VM with a prepared shirudo, for running a test by
    hand.

In CI, the tests workflow runs ``make tests`` and the sanitizer workflow
``make tests VM=0`` under ASan and UBSan with gcc and clang.

4) Unit tests
-------------

``testing/unit/<m>.c`` tests the functions of ``util/<m>.c``.  It calls them
directly and checks the results with the assertions of ``unit.h``:
``EXPECT_*`` records a failure and goes on, ``ASSERT_*`` ends the case.
Each case is a ``static void`` function named for what it shows, and the
file lists its cases in one table that ends with an empty entry::

    static void decode_refuses_non_hex(void)
    {
            __u8 out[1];

            EXPECT_EQ(hex_decode("zz", out, 1), -EINVAL);
    }

    static const struct unit_case cases[] = {
            UNIT_CASE(decode_refuses_non_hex),
            {}
    };

    UNIT_SUITE(hex, cases);

A case left out of the table is an unused static function, which the
build refuses.  Each case runs in a child of its own, so a crash, a
sanitizer report or a hang fails that case alone; the case's stderr comes
back as TAP diagnostics when it fails, and ``unit_skip()`` skips it with a
reason.  The tests link the util objects from an archive, so a test pulls
in only what its module needs and needs no build rule of its own.

A change to a function of ``util/`` adds or updates its cases.  What needs
the command line, root, the TPM simulator, the kernel or an implementation
written apart from shirudo to compare against is a functional test.

5) Functional and end-to-end tests
----------------------------------

A shell test starts in ``testing/``, sources ``test-lib.sh``, and runs its
cases with ``test_expect_success``; ``test_skip_all`` skips the whole script
when a prerequisite is missing.  It works in a scratch directory of its own
and writes nothing into the tree.

A target test also sources ``integration-lib.sh`` and calls
``target_guard``.  Its family's VM was booted for that family alone, so it
finds loaded only what its family loaded; ``require_loaded <name>`` loads a
signed fixture of the bundle that a test needs.  A target test that skips
as a whole fails its family, since every one of them is required.

The end-to-end tests in ``end2end/`` run a shipped package against real
kernel activity and follow it out through the export path.

6) The test VM
--------------

``testing/vm/`` builds and runs the target side.  ``build-kernel.sh``
builds the test kernel from bpf-next or a release tag with the slim
configuration and the patches under ``kernel/``.  ``build-bundle.sh``
builds the operator side: shirudo, the helpers, the signed upatch fixtures,
the dm-verity runtime image and the signed config images, which
``image-fixtures.sh`` and ``bundle-images.sh`` produce.  ``run.sh`` boots a
guest with a software TPM and the bundle and, in test mode, has
``guest.sh`` run one family's tests in it.  ``testing/swtpm.sh`` runs a
software TPM on the host for development without a VM.
