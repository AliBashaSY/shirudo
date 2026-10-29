..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: How-to
:audience: Operators

Initialize a target
===================

``shirudo init`` brings enforcement up on every boot. It runs twice: once
from the initrd, where it does the work, and once from the root file
system, where it checks that work and records it. You do not run it by
hand. This page tells you what each run does, how to check that it
worked, and what to do when it did not.

Before you begin
----------------

- A target set up as described in :doc:`/operate/boot-integration`.
- A root shell on the target. On the target, ``shirudo`` means the
  runtime image's binary, ``/run/shirudo/runtime/usr/bin/shirudo``. See
  :ref:`runtime-invocation`.

What init does
--------------

.. list-table::
   :header-rows: 1
   :widths: 16 22 62

   * - Run
     - Unit and binary
     - What it does
   * - Initrd
     - ``shirudo-initrd.service``, with the binary in the initrd that the
       UKI authenticates
     - Binds ``/sysroot/var/lib/shirudo`` writable at
       ``/var/lib/shirudo``. Mounts the private bpffs. Verifies the
       runtime image against ``runtime.root_hash``, enrolls the config
       certificate in ``.dm-verity`` and seals it. Selects and verifies
       the config image, enrolls the BPF trust set in ``.bpf`` and seals
       it. Labels writable state, attaches the LSM programs, and mounts
       the runtime image at ``/run/shirudo/runtime``. Records a config
       BOOT event in the config PCR, applies the config's revocations,
       restores the archived upatches and binds the measurement register.
       Writes its init record to ``/run/shirudo/eventlog.pending``.
   * - Root file system
     - ``shirudo.service``, with the runtime image's binary through the
       drop-in
     - Checks that the keyring and the runtime image are the ones the
       initrd recorded and that the config activation is complete.
       Appends the pending init record to ``/var/lib/shirudo/eventlog``.
       Adds no register event.

The mounts under ``/run/shirudo`` survive the switch to the root file
system. :doc:`/internals/boot-flow` explains each step and the order.

Check that init worked
----------------------

1. Read the status report:

   .. code-block:: console

      target# shirudo status
      Host:              docs
      Roles:             target
      Mode:              production
      Status:            ok
      Version-ID:        447b34bc8bc5547334ced4c737f987a9d3f8f03e332cf234626f0b81b5955064

      Conditions:
        TYPE             STATUS  DETAIL
        LsmAttached      OK      lsm/bpf link present and pinned
        KeyringSealed    OK      bpf serial=130847582, dm-verity serial=962472513, pins match
        ChainIntact      OK      live register == eventlog replay
        TpmMeasured      OK      NV index 0x01818E00 readable
        ConfigActive     OK      config dc90bacc356f.. is active
      ...
      TPM Measurement:
        TPM:               reachable (device:/dev/tpmrm0)
        NV index:          0x01818E00 (anchored in PCR 9)
        Chain:             dc21c3dabcd2fd0a751c7146ad71c595f2386a4406feb64c3d6e2bd65c5bc706
        Register name:     000b9b7b0f925415fdf0d5676369f8cc9146b9e3142537148b31e79dbffcc96e72f8
        Anchored:          yes (name on record)
        PCR before init:   0000000000000000000000000000000000000000000000000000000000000000
        Phase at init:     0000000000000000000000000000000000000000000000000000000000000000 (PCR 11)
        Bound in:          initrd
        Chain vs eventlog: matches
        Eventlog:          /var/lib/shirudo/eventlog  (3 entries, 2675 bytes)
      ...

   ``Status: ok`` and five ``OK`` conditions mean that enforcement is
   attached, the ``.bpf`` keyring is the one init sealed, the register
   matches the eventlog and the config is active. ``Bound in: initrd``
   says that the initrd run bound the register.

   ``PCR before init`` and ``Phase at init`` are PCR 9 and PCR 11 as init
   found them. The output comes from the test VM, which boots the kernel
   and initrd without a UKI, so both are zero. A target that boots a
   measured UKI shows its own values, and a verifier pins them as
   :doc:`/operate/attest` describes.

2. Check the fields a script reads:

   .. code-block:: console

      target# shirudo status --json

   .. list-table::
      :header-rows: 1
      :widths: 40 60

      * - Field
        - Value after a good boot
      * - ``status``
        - ``"ok"``
      * - ``mode``
        - ``"production"``
      * - ``keyring_mode``
        - ``"bpf"``
      * - ``tpm.bound_in``
        - ``"initrd"``
      * - ``tpm.chain_matches_eventlog``
        - ``true``
      * - ``tpm.register_rebound``
        - ``false``
      * - ``config.activation_pending``
        - ``false``

   :doc:`/reference/formats/status-json` describes every field.

3. Check that the root run adopted the initrd's record:

   .. code-block:: console

      target# ls /run/shirudo/eventlog.pending

   The file must not exist. When it remains, ``shirudo.service`` did not
   run or failed, and its journal says why.

4. Read both runs in the journal:

   .. code-block:: console

      target# journalctl -b -u shirudo-initrd.service -u shirudo.service

   A complete initrd run logs
   ``tpm: register bound in the initrd and extended (index 0x01818E00,
   PCR 9 anchored, <n> upatches in set)`` and
   ``init: complete (LSM active, writable state sealed)``. The root run
   logs ``tpm: register already bound this boot; chain unchanged (index
   0x01818E00)`` and the same last line.

To see which prerequisites init checks first, run the pre-flight alone.
It changes no persistent or enforcement state:

.. code-block:: console

   target# shirudo init --check-only
   pre-flight checks:
     root                   OK
     bpf_lsm_active         OK
     bpffs_supported        OK
     signed_loader_kernel_abi OK
     cert_on_disk           OK
     cert_algorithm         OK
     cert_kernel_support    OK
     kernel_keyring         OK
     embedded_loader        OK
     tpm2_reachable         OK: TCTI override: device:/dev/tpmrm0
   10/10 passed (0 failed)

:doc:`/reference/cli/init` describes each check.

When init fails
---------------

A failed initrd run does not stop the boot. ``shirudo-initrd.service`` is
only wanted by ``initrd.target``, and nothing acts on its failure. The
host boots, and what it runs depends on the step where init stopped.
When the initrd lacks the shirudo module or its certificate, init does
not run at all and nothing reports an error.
``shirudo-gen-initrd --check`` detects such an initrd before you boot it.

.. list-table::
   :header-rows: 1
   :widths: 24 38 38

   * - Where init stopped
     - What you see
     - What to do
   * - Before the LSM programs attach: the pre-flight, the image
       settings, the bpffs, verifying the images and enrolling the keys,
       sealing state or loading enforcement.
     - The host runs without enforcement. ``shirudo.service`` is
       skipped, because the runtime image is not mounted. The keepalive
       service cannot start without the runtime binary, so the keepalive
       stops. ``status`` from any copy of the binary reports ``lsm-off``.
     - Read the initrd journal, fix the cause and reboot.
   * - After the LSM programs attach and before the config BOOT event:
       mounting the runtime image or selecting the config.
     - Enforcement is on. Depending on the step, the runtime binary is
       missing, commands fail with ``shirudo: image state: <error>``, or
       ``status`` reports a config condition that does not belong to
       this boot.
     - Keep the journal, fix the cause and reboot. When the journal does
       not explain the state, follow :doc:`/operate/incident-tampered`.
   * - After the config BOOT event: restoring upatches, binding the
       register or completing the boot.
     - Enforcement is on and the config activation stays pending.
       ``status`` reports ``reboot-pending``, and ``ChainIntact`` and
       ``TpmMeasured`` warn that no register is bound. Commands other
       than ``status`` and ``image status`` fail with
       ``shirudo: image state: Operation now in progress``, attestation
       included.
     - Fix the cause and reboot. A reboot without a fix fails at the
       same step.

Do not run ``shirudo init`` again in the same boot. After a failure past
the config BOOT event, init refuses with
``image: activation is pending; further changes are blocked`` and
``init: loading authenticated boot settings failed: Operation now in
progress``. Only a reboot clears the pending activation. A re-run after a
successful init changes nothing.

Do not run init from the root file system to make up for a failed initrd
run either. Workloads have already run without enforcement by then, and
the register would record a binding from the root file system, which a
verifier that pins PCR 11 rejects. Reboot instead.

Failures to restore upatches
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Init restores the archived upatches in name order and stops at the first
one that fails, with ``boot-restore: failed to apply <file>`` and
``init: boot-restore failed; further changes and attestation remain
blocked``. The upatches it loaded before that one stay loaded, and no
register measures them. The archive is sealed, so the next boot tries
the same upatch again. The usual cause is a kernel on which the upatch
no longer loads. :ref:`boot-restore-lockout` describes the way out.

Register binding failures
~~~~~~~~~~~~~~~~~~~~~~~~~

When init cannot bind the register, it logs
``init: incomplete -- LSM active and upatches loaded, but TPM register
not advanced`` after one of these lines:

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Message
     - Cause and action
   * - ``tpm: not usable; shirudo requires a TPM 2.0 (tpm_error)``
     - No usable TPM answered on ``/dev/tpmrm0``. Check the TPM driver in the
       initrd and the firmware's TPM setting.
   * - ``tpm: register 0x01818E00 cannot be bound (tpm_error); ...``
     - The TPM refused the authorization, usually because the owner
       hierarchy has a password that shirudo does not have. Set
       ``tpm.owner_auth`` in both bootstrap files as
       :doc:`/operate/boot-integration` describes. The environment
       variable that the message names is refused on a target.
   * - ``tpm: register extend failed -- chain NOT advanced while LSM
       active (tpm_error); operator must reconcile``
     - The register could not be defined or extended. Read the journal
       for the TPM error before it.
   * - ``tpm: the register this boot recorded is gone or replaced;
       refusing to bind another before the next TPM reset (tampered)``
     - Something removed or replaced the register after init bound it
       this boot. Handle it as an incident:
       :doc:`/operate/incident-tampered`.

The threat model lists the boot paths that leave a target unprotected and
what detects them: :doc:`/security/threat-model`.

Next steps
----------

- :doc:`/operate/read-status` for every section and verdict of the
  status report.
- :doc:`/operate/troubleshooting` for refusals after init.
- :doc:`/operate/attest` to prove the boot to a remote verifier.
