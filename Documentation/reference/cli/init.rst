..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: Operators

shirudo init
============

Bring up enforcement on a target: enroll and seal the keyrings, attach the
LSM programs, restore the archived upatches and bind the measurement
register.

Runs on: target, as root. In production ``shirudo-initrd.service`` runs it
from the initrd and ``shirudo.service`` runs it again with the runtime
binary after the switch to the root file system. The image gate does not
apply. Requires: the pre-flight checks below, the bootstrap settings in
``/etc/shirudo/shirudo.conf``, and for the first run in a boot the kernel
parameters ``bpf.keyring_unsealed=1`` and ``dm_verity.keyring_unsealed=1``.
Changes measured state: yes.

Usage
-----

.. code-block:: none

   shirudo init [--check-only]

Description
-----------

#. ``init`` loads the authenticated bootstrap settings: from
   ``/etc/shirudo/shirudo.conf``, which must name the runtime image and its
   root hash, or from the frozen identity when enforcement is already up.
#. It runs the pre-flight checks and prints them. With ``--check-only`` it
   stops here.
#. In the initrd (``/etc/initrd-release`` exists), it requires a tmpfs or
   ramfs root and binds ``/sysroot/var/lib/shirudo`` writable over
   ``/var/lib/shirudo``. The bind is detached when ``init`` exits.
#. It mounts the private bpffs at ``/run/shirudo/bpffs``.
#. On the first run in a boot it mounts the runtime image, enrolls the
   config certificate into the ``.dm-verity`` keyring and seals it,
   selects the signed config for this boot, then enrolls the trusted
   certificate set into the ``.bpf`` keyring and seals it. On a later run
   it checks that the frozen identity names the same ``.bpf`` keyring and
   runtime image.
#. It labels the persistent state, adopts the eventlog that the initrd
   run left in ``/run/shirudo/eventlog.pending``, and loads and pins the
   enforcement programs.
#. It mounts the authenticated runtime image at ``/run/shirudo/runtime``.
#. It begins the config ``BOOT`` event: it records the selection as
   pending, extends the config PCR named in the bootstrap settings and
   mounts the config image at ``/run/shirudo/config``.
#. It restores every upatch archived in ``/var/lib/shirudo/upatches``,
   all or nothing. An upatch already loaded with the archived payload is
   kept. An archived upatch that is not loaded after this boot's register
   was bound is refused as tampered.
#. It binds the measurement register under PCRs 9 and 11 and records one
   init event that covers the certificate set, the restored upatches and
   the upatch tree root.
#. It completes the config ``BOOT`` event.

Measurement: the config PCR receives one ``BOOT`` event per boot. The
measurement register receives one init event per boot. A second run in the
same boot after a successful first one adds no event.

A failure after the ``BOOT`` event began, in the restore or in binding the
register, leaves the config activation pending. The help text of
``init`` calls it idempotent, but in that state a second run fails and the
image gate refuses every command except ``status`` until the next reboot.
The next boot restores the same archive. See
:doc:`/operate/troubleshooting`.

Pre-flight checks
~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Check
     - Passes when
   * - ``root``
     - ``init`` runs as root.
   * - ``bpf_lsm_active``
     - The BPF LSM is active.
   * - ``bpffs_supported``
     - The kernel supports bpffs.
   * - ``signed_loader_kernel_abi``
     - The kernel has the signed-loader interface.
   * - ``cert_on_disk``
     - The primary certificate exists: ``/etc/shirudo/keys/cert.der`` in
       the initrd, ``/run/shirudo/runtime/keys/cert.der`` with an active
       config image, ``/var/lib/shirudo/keys/cert.der`` otherwise.
   * - ``cert_algorithm``
     - Every trusted certificate holds an ML-DSA-87 key.
   * - ``cert_kernel_support``
     - The kernel can parse every trusted certificate. The check adds
       each to the thread keyring and clears it again.
   * - ``kernel_keyring``
     - The kernel has keyrings and a ``.bpf`` keyring.
   * - ``embedded_loader``
     - The enforcement BPF objects embedded in the binary are present.
   * - ``tpm2_reachable``
     - A TPM answers on the configured TCTI.

``--check-only`` changes no persistent or enforcement state. It reads the
TPM and probes the keyring as described.

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 22 12 12 54

   * - Option
     - Argument
     - Default
     - Description
   * - ``--check-only``
     - none
     - off
     - Run and print the pre-flight checks, then exit.

Exit status
-----------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Status
     - Meaning
   * - 0
     - Enforcement is up, or with ``--check-only`` every check passed.
   * - 1
     - The bootstrap settings cannot be loaded
       (``init: authenticated image settings: <error>``), a pre-flight
       check failed (``init: pre-flight failed (<n>/<m> checks)``), or a
       phase failed (``init: <phase> failed: <error>``).
   * - 2
     - An unknown option.

Files and environment
---------------------

- Reads ``/etc/shirudo/shirudo.conf``, ``/etc/shirudo/keys/`` in the
  initrd, the runtime and config images the bootstrap settings name,
  ``/var/lib/shirudo/images``, ``/var/lib/shirudo/upatches``, ``/var/lib/shirudo/eventlog`` and
  ``/var/lib/shirudo/config-update``.
- Writes ``/run/shirudo/bpffs``, ``/run/shirudo/eventlog.pending`` in the
  initrd, ``/var/lib/shirudo/eventlog`` and the config activation records
  in ``/var/lib/shirudo/config-update``.
- ``SHIRUDO_INIT_ARGS`` in ``/etc/shirudo/init.env`` is passed by the
  shipped ``shirudo.service``. The runtime drop-in replaces that command
  line with ``init`` alone.

Examples
--------

Run the pre-flight checks on a target:

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

Related
-------

- :doc:`/operate/initialize`
- :doc:`/internals/boot-flow`
- :doc:`/operate/boot-integration`
- :doc:`status`
