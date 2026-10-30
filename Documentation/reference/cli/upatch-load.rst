..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: Operators

shirudo upatch load
===================

Load a signed ``.upatch`` into the target kernel: verify the signature, run
the loader, pin the resulting links and extend this boot's measurement
register.

Runs on: target, as root with the runtime binary. Requires: ``init``
completed this boot. Changes measured state: yes, one ``upatch_load``
event.

Usage
-----

.. code-block:: none

   shirudo upatch load [--expect-chain=<register>] [--expect-version-id=<version-id>]
                       [--no-precheck] [--no-version-check] [--report=<path>]
                       <file>

Every option with a value takes it after ``=``. ``--help`` anywhere on the
line prints the help. Progress and error messages go to standard error,
so with ``--report=-`` standard output carries only the report.

Description
-----------

#. With ``--expect-version-id``, ``load`` compares the value with this
   binary's ``SHIRUDO_VERSION_ID`` and refuses on a difference
   (``version_mismatch``).
#. It copies the file to a private staging file and reads it once. Every
   later step uses that copy.
#. Pre-flight, unless ``--no-precheck``: the kernel has the
   signed-loader interface (``kernel_abi_unsupported``), ``init`` froze a
   trust anchor this boot (``keyring_missing``), the ``.bpf`` keyring is
   the frozen one and the LSM link is pinned (``tampered``), and with
   ``--expect-chain`` the register holds that value (``tampered``,
   message ``chain_drift``).
#. It refuses when the eventlog would exceed ``eventlog.max_bytes``
   (``eventlog_full``).
#. It parses the artifact: a LOAD artifact, a safe and signed name, a
   payload digest that matches the payload, valid attach metadata
   (``parser_rejected``). For a uprobe it checks each target binary
   against its recorded build ID (``target_verify`` phase).
#. It loads the loader with ``BPF_PROG_LOAD`` against the ``.bpf``
   keyring. The kernel checks the signature before the verifier runs
   (``signed_loader_load`` phase).
#. It runs the loader, which loads the payload programs and maps
   (``payload_load`` phase).
#. It attaches every program and pins the links under
   ``/run/shirudo/bpffs/upatches/<name>/``, where a dot in the name becomes
   ``~d``, with the payload digest in the directory's
   ``security.bpf.shirudo.payload`` extended attribute (``link_create``
   phase).
#. It copies the artifact to ``/var/lib/shirudo/upatches/<name>.upatch``,
   the archive that ``init`` restores at boot (``persist`` phase).
#. It extends the register with the ``upatch_load`` event for the name and
   payload digest, and appends the event to the eventlog (``tpm_extend``
   phase).

When the copy to the archive or the register extend fails, ``load``
removes the pins and the archive again, so that no upatch runs
unmeasured. When the extend succeeded and only the eventlog append failed,
the upatch stays loaded and the report says ``tpm_error``.

``--no-precheck`` skips step 3 only. The kernel signature check and the
LSM still apply. ``bpm`` passes it when it loads the upatches of a
package.

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 24 20 10 46

   * - Option
     - Argument
     - Default
     - Description
   * - ``--expect-chain``
     - ``<register>``, 64 hex characters
     - none
     - Refuse unless the register holds this value before the load, for
       example ``register_at_boot`` from :doc:`attest-predict`.
   * - ``--expect-version-id``
     - ``<version-id>``
     - none
     - Refuse unless this binary's ``SHIRUDO_VERSION_ID`` equals the
       value (string comparison).
   * - ``--no-version-check``
     - none
     - off
     - Skip the ``--expect-version-id`` comparison.
   * - ``--no-precheck``
     - none
     - off
     - Skip the pre-flight checks, ``--expect-chain`` included, with a
       warning.
   * - ``--report``
     - ``<path>`` or ``-``
     - none
     - Write the JSON report to ``<path>`` (mode 0644), or to standard
       output with ``-``. A path in sealed state is refused.

.. _upatch-report:

Report
------

``--report`` writes one JSON object, also when the command fails:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - Value
   * - ``schema``
     - ``1``
   * - ``result``
     - ``ok`` or ``failed``
   * - ``op``
     - ``upatch_load`` or ``upatch_unload``
   * - ``phase``
     - ``pre_flight``, ``parse``, ``target_verify``,
       ``signed_loader_load``, ``payload_load``, ``link_create``, ``pin``,
       ``persist``, ``tpm_extend`` or ``done``
   * - ``errno``, ``errno_name``
     - the error number and its name, or ``null``
   * - ``category``
     - the failure category, ``ok`` on success
   * - ``message``
     - a short description
   * - ``verifier_log``
     - the kernel verifier's log, when it gave one
   * - ``loader_return``
     - the loader's return value
   * - ``upatch``
     - ``path``, ``name`` and ``payload_sha256`` of the artifact
   * - ``kernel``
     - ``release``, the running kernel
   * - ``tpm``
     - ``nv_index`` and ``chain_after``, the register after the event, on
       success, ``null`` otherwise

Exit status and failure categories
----------------------------------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Status
     - Meaning
   * - 0
     - The upatch is loaded and measured.
   * - 1
     - Refused or failed. The report and the message name the category.
   * - 2
     - A usage error: an unknown option, a second file, no file, or an
       ``--expect-chain`` that is not 64 hexadecimal characters. Also the
       category ``attach_target_missing``.

``load`` reports these categories: ``version_mismatch``,
``kernel_abi_unsupported``, ``keyring_missing``, ``tampered``,
``eventlog_full``, ``parser_rejected``, ``signature_rejected``,
``signature_unsupported``, ``lsm_denied``, ``verifier_rejected``,
``attach_target_missing``, ``binary_mismatch``,
``attach_scope_mismatch``, ``tpm_error`` and ``unknown``.
:doc:`/reference/failure-categories` gives the cause and the action for
each. Handle ``tampered`` as an incident
(:doc:`/operate/troubleshoot/incident-tampered`).

Files and environment
---------------------

- Reads the artifact, ``/var/lib/shirudo/eventlog`` and
  ``eventlog.max_bytes``
- Writes ``/run/shirudo/bpffs/upatches/<name>/``,
  ``/var/lib/shirudo/upatches/<name>.upatch``, the eventlog and the
  register

Examples
--------

Load an artifact and print the report:

.. code-block:: console

   target# shirudo upatch load --report=- shirudo-test-basic.upatch
   loaded signed loader: prog_fd=10 keyring=420195959 sig=4853B insns=124 data=2400B
   loader ok, retval=0 (maps=0 progs=1)
   attached + pinned prog 0 (raw_tracepoint) -> /run/shirudo/bpffs/upatches/shirudo-test-basic/link
   upatch load: name=shirudo-test-basic keyring=420195959 progs=1
   upatch load: chain extended, value=c50b22ac27c5bb6a90ee5ff9b440fbbd5bb60b84a2aa7b395daec3f330886fcd
   upatch load measured: name=shirudo-test-basic chain=c50b22ac27c5bb6a90ee5ff9b440fbbd5bb60b84a2aa7b395daec3f330886fcd
   {"schema":1,"result":"ok","op":"upatch_load","phase":"done","errno":null,"errno_name":null,"category":"ok","message":"","verifier_log":"","loader_return":0,"upatch":{"path":"shirudo-test-basic.upatch","name":"shirudo-test-basic","payload_sha256":"8aee7072a1a15e4dddc93ad6b253a0e2e70ad6cdf767442aa8a1c1997bc3069f"},"kernel":{"release":"7.3.0-bpf-next+"},"tpm":{"nv_index":"0x01818E00","chain_after":"c50b22ac27c5bb6a90ee5ff9b440fbbd5bb60b84a2aa7b395daec3f330886fcd"}}

Load only when the register holds the value you predicted:

.. code-block:: console

   target# shirudo upatch load --expect-chain=<register> fix.upatch

``<register>`` is the ``register_at_boot`` or ``register_after_events``
line of :doc:`attest-predict` for the state you expect. When the register
differs, ``load`` refuses before it touches the kernel:

.. code-block:: none

   upatch load: pre-flight refused (category: tampered, message: chain_drift) -- live chain c50b22ac27c5bb6a90ee5ff9b440fbbd5bb60b84a2aa7b395daec3f330886fcd != expected; re-status and re-issue with the refreshed value

The message suggests repeating the command with the live value. Do not.
A register that differs from your prediction is an incident
(:doc:`/operate/troubleshoot/incident-tampered`).

A refusal for an artifact signed by a key the target does not trust:

.. code-block:: console

   target# shirudo upatch load --report=- shirudo-test-foreign.upatch
   BPF_PROG_LOAD (signed loader): Required key not available
   loader verifier log:
   signature verification failed: -126

The report of that refusal names ``"phase":"signed_loader_load"``,
``"errno_name":"ENOKEY"`` and ``"category":"keyring_missing"``.

Related
-------

- :doc:`upatch-unload`
- :doc:`build`
- :doc:`attest-predict`
- :doc:`/reference/failure-categories`
- :doc:`/internals/signed-loader`
- :doc:`/upatches/test/debugging`
