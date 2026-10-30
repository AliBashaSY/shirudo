..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: Operators

shirudo attest verify
=====================

Check an evidence document offline against the pinned keys and the state
you expect the target to be in.

Runs on: operator host, offline. It uses no TPM. Requires: the evidence
and the nonce it was requested with. Changes measured state: no.

Usage
-----

.. code-block:: none

   shirudo attest verify --evidence=<evidence> --nonce=<nonce>
                         [--ak-name=<name>] [--ek-name=<name>]
                         [--expect-credential=<credential>]
                         [--cert=<certificate>]... [--upatch=<file>]...
                         [--expect=<name>=<digest>]... [--tree-root=<digest>]
                         [--expect-chain=<register>] [--allow-runtime-events]
                         [--expect-config-root=<root> --expect-config-pcr=<pcr>]
                         [--expect-pcr=<pcr>=<value>]...
                         [--expect-bind-pcr=<pcr>=<value>]...
                         [--strict-anchor] [--require-initrd]

Every option takes its value after ``=``. On a host where enforcement is up
the image gate refuses ``--cert``. Run ``verify`` where enforcement is not
up.

Description
-----------

``verify`` prints one line per check, ``ok:``, ``FAIL:`` or ``note:``,
followed by ``verdict: ok`` or ``verdict: FAIL``. It checks:

- the nonce, in the document, the quote and the register certification
- the attestation key: restricted, fixed to its TPM, its name matching its
  public area and, with ``--ak-name``, the pinned name
- the endorsement key: its name and the TCG default template, and with
  ``--ek-name`` the pinned name. With ``--expect-credential``, the
  credential the target recovered
- the quote and the register certification: signed by the attestation key
  as an endorsement primary, covering the listed PCRs and index, of one
  TPM reset
- the config root history against the quoted config PCR, and with
  ``--expect-config-root`` the approved root
- the register's public area against the recorded PCR 9 and 11 binding,
  the init record of this TPM reset, the eventlog replay to the certified
  value and the anchor in PCR 9
- the expected state: with ``--cert`` (and ``--upatch``, ``--expect``,
  ``--tree-root``) the init event, and the events after boot, which fail
  unless ``--allow-runtime-events``. With ``--expect-chain``, the register
  value

Without ``--ak-name``, ``verify`` warns that the signer is not pinned, and
a document from any TPM passes the key checks. Without ``--cert`` or
``--expect-chain``, it compares no state and prints the register value as
a note. A ``verdict: ok`` then says only that the document is consistent
in itself. Pin the keys at enrollment (:doc:`attest-challenge`) and always
give the expected state.

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 26 24 50

   * - Option
     - Argument
     - Description
   * - ``--evidence``
     - ``<evidence>``, a file
     - Required. The document ``shirudo attest`` wrote.
   * - ``--nonce``
     - ``<nonce>``, hexadecimal, up to 64 bytes
     - Required. The nonce the evidence was requested with.
   * - ``--ak-name``
     - ``<name>``, 68 hex characters
     - The attestation key name pinned at enrollment.
   * - ``--ek-name``
     - ``<name>``, 68 hex characters
     - The endorsement key name pinned at enrollment.
   * - ``--expect-credential``
     - ``<credential>``, 64 hex characters
     - The credential of the challenge given to the target. Needs
       ``--ak-name``.
   * - ``--cert``
     - ``<certificate>``, DER
     - A certificate the target enrolls. Repeat for each, up to 16.
   * - ``--upatch``
     - ``<file>``
     - An artifact archived on the target at boot. Repeatable. Needs
       ``--cert``.
   * - ``--expect``
     - ``<name>=<digest>``
     - An archived upatch by name and payload digest. Repeatable. Needs
       ``--cert``.
   * - ``--tree-root``
     - ``<digest>``, 64 hex characters
     - The upatch tree projection root at boot. Needs ``--cert``.
   * - ``--expect-chain``
     - ``<register>``, 64 hex characters
     - The register value after the expected events.
   * - ``--allow-runtime-events``
     - none
     - Accept loads, unloads and revocations after boot.
   * - ``--expect-config-root``
     - ``<root>``, 64 hex characters
     - The approved config image root hash. Needs
       ``--expect-config-pcr``.
   * - ``--expect-config-pcr``
     - ``<pcr>``, 0 to 15, not 9 or 11
     - The PCR the config activation extends.
   * - ``--expect-pcr``
     - ``<pcr>=<value>``, PCR 0 to 23
     - A PCR value the quote must carry. Repeatable.
   * - ``--expect-bind-pcr``
     - ``<pcr>=<value>``, PCR 0 to 23
     - A PCR value the register was bound under. Repeatable.
   * - ``--strict-anchor``
     - none
     - Require PCR 9 to hold only the anchor.
   * - ``--require-initrd``
     - none
     - Fail unless ``init`` bound the register from the initrd.

Exit status
-----------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Status
     - Meaning
   * - 0
     - ``verdict: ok``.
   * - 1
     - ``verdict: FAIL``.
   * - 2
     - A usage error or an invalid value, ``--evidence`` or ``--nonce``
       missing, an option that needs another, an unreadable evidence file
       or a file that is not a shirudo evidence document.

Examples
--------

Verify evidence against the predicted register value, with the
attestation key pinned:

.. code-block:: console

   operator$ shirudo attest verify --evidence=ev.json --nonce=$NONCE --ak-name=<ak-name> --expect-chain=<register>
   ok:   nonce matches the request
   ok:   attestation key: restricted, fixed to this TPM
   ok:   attestation key: name matches its public area
   ok:   attestation key: the pinned key
   ok:   endorsement key: name matches its public area
   ok:   endorsement key: TCG default template under the endorsement policy
   ok:   quote: ECDSA signature by the attestation key
   ok:   quote: signed by the key as an endorsement primary
   ok:   quote: carries the nonce
   ok:   quote: covers the PCRs the document lists
   ok:   quote: PCR digest matches the listed values
   ok:   register: ECDSA signature by the attestation key
   ok:   register: signed by the key as an endorsement primary
   ok:   register: carries the nonce
   ok:   register: certification names the listed index
   ok:   register: certified contents match the listed value
   ok:   quote and certification are of one TPM reset
   ok:   config: config root history matches the quoted PCR
   note: config root has no operator approval; use --expect-config-root with --expect-config-pcr
   note: config records a selection; direct TPM writer protection is deferred
   ok:   eventlog: this boot has an init record
   ok:   register: the document names index 0x01818E00 and PCRs 9 and 11
   ok:   register: public area is bound to PCRs 9 and 11 as recorded
   note: binding: made in the initrd
   ok:   eventlog: one init record for this TPM reset (1 this boot)
   ok:   register: init record is of the TPM's current reset count
   ok:   eventlog: 2 records replay to the certified value
   ok:   anchor: record digest matches the index name
   ok:   anchor: PCR 9 equals the recorded value extended by the anchor
   ok:   state: register equals the expected value
   note: event after boot: upatch_load basic
   verdict: ok

``$NONCE`` holds the nonce given to ``shirudo attest``, ``<ak-name>`` the
name pinned at enrollment and ``<register>`` the
``register_after_events`` line of :doc:`attest-predict`.

Verify against the enrolled certificates and require an initrd binding.
Events after boot fail the check without ``--allow-runtime-events``:

.. code-block:: console

   operator$ shirudo attest verify --evidence=ev.json --nonce=$NONCE --ak-name=<ak-name> --cert=cert.der --require-initrd --strict-anchor

The end of that report, for a target that loaded and unloaded a upatch
after boot:

.. code-block:: none

   ok:   binding: made while the initrd ran
   ok:   eventlog: one init record for this TPM reset (1 this boot)
   ok:   register: init record is of the TPM's current reset count
   ok:   eventlog: 3 records replay to the certified value
   ok:   anchor: record digest matches the index name
   ok:   anchor: PCR 9 equals the recorded value extended by the anchor
   FAIL: state: init event covers the expected certificate, set and tree root
   FAIL: state: 2 events after boot change the register
   note: event after boot: upatch_load shirudo-test-basic
   note: event after boot: upatch_unload shirudo-test-basic
   verdict: FAIL

The first ``FAIL`` line says that the target enrolls a certificate set
other than the one given: that target enrolls two certificates and the
command names one.

Related
-------

- :doc:`attest`
- :doc:`attest-predict`
- :doc:`attest-challenge`
- :doc:`/operate/run/attest`
- :doc:`/security/threat-model`
