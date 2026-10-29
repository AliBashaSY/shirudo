..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: How-to
:audience: Operators

Manage signing keys
===================

A target trusts a small set of keys, and each one enters at a different
time. Some are fixed when the target is first provisioned, some change
with a signed config, and some change only with a new runtime image and a
reboot. This page lists the keys, shows what can change when, and walks
through vouching for a signer and revoking one.

Plan the BPF signers before the first boot. A target that is already
initialized cannot enroll a BPF signer that its first config did not
enroll.

Before you begin
----------------

- The operator host with the operator package, and the key directories
  you created with ``shirudo genkey``. Each holds ``signing_key.pem``
  (mode ``0600``, the signing helper refuses any other mode) and
  ``cert.der``.
- The source tree of the config image the target runs, as built in
  :doc:`/operate/boot-integration`.
- Root on the target. On the target, ``shirudo`` is the runtime image's
  binary, as :ref:`runtime-invocation` sets up.

Key inventory
-------------

.. list-table::
   :header-rows: 1
   :widths: 22 28 50

   * - Key
     - Where the target takes it from
     - What it authorizes
   * - Runtime primary BPF certificate
     - ``keys/cert.der`` in the runtime image, read as
       ``/run/shirudo/runtime/keys/cert.der``
     - Signed loaders of upatches. It is also the only certificate that
       verifies upatch tree publications, whatever the config revokes or
       vouches for.
   * - Vouched BPF certificates
     - ``keys/trusted/<name>.der`` with ``keys/trusted/<name>.vouch`` in
       the config image
     - Signed loaders of upatches. A certificate joins the set when its
       vouch verifies under a certificate already in the set, starting
       from the runtime primary. The set holds at most 16 certificates.
   * - Revocation records
     - ``keys/revoked/<fingerprint>.der`` in the config image
     - Nothing. Each record removes the certificate it holds from the
       set.
   * - Config verification certificate
     - ``etc/shirudo/config-cert.der`` in the runtime image, named by
       ``config.verification_key``
     - Config images. Init enrolls it in the kernel's ``.dm-verity``
       keyring and seals that keyring around it.
   * - Kernel trusted keys
     - The kernel's built-in trusted keyring, and the secondary and
       platform keyrings when the kernel enables them for dm-verity
     - Config images as well. The kernel checks these keyrings before
       ``.dm-verity``, so their keys, including the kernel build's module
       signing key, can sign a config the target activates.
       :doc:`/operate/hardening` lists the kernel options.
   * - Repository index certificates
     - ``etc/shirudo/bpm/keys/`` in the config image, named by
       ``key_file`` in ``etc/shirudo/bpm/registries.d/``
     - The package index of that repository. shirudo does not require
       ML-DSA-87 for them.
   * - Attestation key (AK) and endorsement key (EK)
     - The TPM. Both are primaries under the endorsement hierarchy with
       fixed templates, so one TPM always yields the same keys.
     - The AK signs the evidence. The EK proves at enrollment that the AK
       sits in the same TPM. You record both names when you enroll the
       target, as :doc:`/operate/attest` describes.
   * - NV writer key
     - The TPM. Init creates an RSA-3072 key from a random seed at every
       boot and freezes its public key in the kernel.
     - Extensions of the measurement register. The init record carries
       its public key as ``nv_writer_public``.

The BPF signing keys and the config signing key are ML-DSA-87 keys from
``shirudo genkey``. The config signing key may be one of the BPF signing
keys or a separate key. Keep the private signing keys on the operator
host. A target needs none, and ``shirudo status`` on a target reports
``keys/signing_key.pem absent``.

What can change when
--------------------

.. list-table::
   :header-rows: 1
   :widths: 22 22 26 30

   * - Key
     - First provisioning
     - Signed config (``image update``)
     - New runtime image, UKI and reboot
   * - Runtime primary BPF certificate
     - Set by the runtime image.
     - Cannot change it. The config can revoke it.
     - Changes it. Every certificate in the selected config's
       ``keys/trusted`` must chain from the new primary, or init fails
       before enforcement starts. Every archived upatch must be signed
       by a key in the new set, or the boot restore fails.
   * - Vouched BPF certificates
     - Enrolled from the initial config.
     - Refused when the config adds a certificate that is not in the
       ``.bpf`` keyring.
     - Not enrolled. The boot selects the config the activation record
       names, and that config passed the live check.
   * - Revocation records
     - Applied at boot.
     - Applied at activation.
     - Kept, as part of the selected config.
   * - Config verification certificate
     - Set by the runtime image.
     - Cannot change it.
     - Init fails at that boot, before enforcement starts: the selected
       config carries the old key's signature, and the sealed
       ``.dm-verity`` keyring holds only the new certificate.
   * - Repository index certificates
     - From the initial config.
     - Change at activation.
     - Kept, as part of the selected config.
   * - AK and EK
     - Fixed by the TPM.
     - No change.
     - No change. A different TPM needs a new enrollment.
   * - NV writer key
     - Created at boot.
     - No change.
     - A new key at every boot.

The first provisioning is the first boot with enforcement. Init has no
activation record yet and selects the config that ``config.image`` in the
runtime bootstrap file names. From then on the activation record selects
the config, and changing ``config.image`` has no effect.

Check what a target trusts
--------------------------

1. Show the certificates, the trusted set and the ``.bpf`` keyring:

   .. code-block:: console

      target# shirudo keys
      Cert (/run/shirudo/runtime/keys/cert.der):
        Subject:               /CN=shirudo
        Issuer:                /CN=shirudo  (self-signed)
        Public-key:            ML-DSA-87
        Fingerprint:           SHA256:d95db97ff4658b96b8b538afef41a25ca9f7a2c4d0abffdb26cbac7b0ba294b9
        SPKI fingerprint:      SHA256:d1dc6624ee6bd0c277864fab0a8a15f8509308aa2338768cb772117c157f8d61
        Valid from:            2026-10-29T16:16:30Z
        Valid until:           2036-10-26T16:16:30Z
        Expires in:            9y 364d

      Signing key (/run/shirudo/runtime/keys/signing_key.pem):
        Present:               no  (target host -- private key lives on the operator)

      Trusted set (2 members, 0 revoked, 0 unvouched):
        2edb279c05efee6b489a65ae07fd6d402a54b6e495be68f6d364544e9b5a37e2
          sha256 3702e8ffe817ade6151eea338194649cc3a5be1e0b940e462a1a75e52f7095d0
          vouched by d1dc6624ee6bd0c277864fab0a8a15f8509308aa2338768cb772117c157f8d61
        d1dc6624ee6bd0c277864fab0a8a15f8509308aa2338768cb772117c157f8d61  (primary)
          sha256 d95db97ff4658b96b8b538afef41a25ca9f7a2c4d0abffdb26cbac7b0ba294b9

      Keyring (.bpf):
        Serial:                420195959
        Frozen serial:         420195959  (matches)
        Members (2):
          serial=410418068   type=asymmetric  shirudo: d10a391a9f1067ed9f64e4ea9b738cf9258b478a
          serial=746517937   type=asymmetric  shirudo: 6935ce2048ec5a5ed607db8f5ac1c005c29f32a4

   ``Cert`` is the runtime primary. The trusted set lists each member by
   its SPKI fingerprint, with the SHA-256 of its DER and the member that
   vouched for it. ``Keyring (.bpf)`` is what the kernel verifies signed
   loaders against: the serial init froze, and one key per
   member.

2. Read the same counts from the status report:

   .. code-block:: console

      target# shirudo status
      ...
      Keyring (.bpf):
        Serial:                420195959
        Frozen serial:         420195959  (matches)
        Certificates:          2 trusted, 0 revoked, 0 unvouched, 2 in the bpf keyring
      ...

   ``unvouched`` counts certificates under ``keys/trusted`` without a
   valid vouch. A config with one is refused, so it stays 0 on a target
   that runs a config.

Vouch for a standby signer
--------------------------

A standby signer is a second BPF signing key that the initial config
enrolls, so that you can revoke the primary later and keep signing
upatches. Tree publications stay with the runtime primary, as `Limits`_
explains. Do this before the target's first boot with enforcement.

1. Create the standby key in a directory of its own:

   .. code-block:: console

      operator$ shirudo genkey --keys-dir=<standby-keys-dir>

2. Sign a vouch for its certificate with the primary key:

   .. code-block:: console

      operator$ shirudo keys vouch <standby-keys-dir>/cert.der \
          -o standby.vouch --keys-dir=<primary-keys-dir>

   The command prints the fingerprint of the certificate it vouched for.
   It refuses to vouch for the signing key's own certificate.

3. Put the certificate and the vouch into the config source tree under
   one name:

   .. code-block:: console

      operator$ mkdir -p config-root/keys/trusted
      operator$ cp <standby-keys-dir>/cert.der \
          config-root/keys/trusted/<standby-fingerprint>.der
      operator$ cp standby.vouch \
          config-root/keys/trusted/<standby-fingerprint>.vouch

   ``<standby-fingerprint>`` is the fingerprint that step 2 printed. Init
   pairs ``<name>.der`` with ``<name>.vouch`` by name. It refuses the whole
   config when a certificate under ``keys/trusted`` has no vouch that
   verifies.

4. Build, sign and place the initial config as
   :doc:`/operate/boot-integration` describes.

5. After the first boot, check that ``shirudo keys`` on the target lists
   the standby signer in the trusted set and the ``.bpf`` keyring.

On a target, ``shirudo keys add`` and ``shirudo bpm key add`` refuse to
change the trusted set, and ``shirudo keys revoke`` always refuses. The
signed config is the only way in.

Revoke a signer
---------------

Revocation goes through a signed config. Activation removes the key
from the kernel at once.

1. Print the fingerprint of the certificate to revoke:

   .. code-block:: console

      operator$ shirudo bpm key fingerprint <revoked-cert.der>

   The command prints the 64-character SPKI fingerprint, the same value
   that ``shirudo keys`` prints.

2. Add a revocation record to the config source tree:

   .. code-block:: console

      operator$ mkdir -p config-root/keys/revoked
      operator$ cp <revoked-cert.der> config-root/keys/revoked/<fingerprint>.der

   The file name must be the lowercase fingerprint with ``.der``, and the
   file must hold that certificate. Init and ``image update`` refuse any
   other entry under ``keys/revoked``.

3. Build and sign the config under a new prefix, and check it offline:

   .. code-block:: console

      operator$ shirudo-gen-image --source config-root --output revoke-1 \
          --keys-dir <config-keys-dir> --kind config
      operator$ shirudo image verify revoke-1 --cert <config-keys-dir>/cert.der

4. Copy ``revoke-1.img``, ``revoke-1.verity``, ``revoke-1.roothash`` and
   ``revoke-1.p7s`` to the target.

5. Activate the config on the target:

   .. code-block:: console

      target# shirudo image update <path>/revoke-1

   ``<path>`` is where you copied the files. ``image update`` also takes
   an ``https://`` prefix. It prints nothing on success.

6. Check the result:

   .. code-block:: console

      target# shirudo keys
      target# shirudo status --include-eventlog

   The trusted set no longer lists the key, the revoked count went up, and
   the eventlog shows a ``key_revoke`` record followed by an
   ``upatch_unload`` record for each loaded upatch the key signed.

Leaving a certificate out of a config does not revoke it: its key stays
in the ``.bpf`` keyring until the next boot. Revoke it with a record.

``image update`` validates the whole config before it measures anything.
It refuses a config whose set would be empty after the revocation, a
certificate under ``keys/trusted`` without a valid vouch, and a member
that is not in the ``.bpf`` keyring. A refused config changes nothing.

What a revocation does
~~~~~~~~~~~~~~~~~~~~~~

At activation, in this order:

1. shirudo records an ``UPDATE`` event for the config in the config PCR.
2. When the revoked key is in the ``.bpf`` keyring, shirudo unlinks it,
   extends the measurement register with a ``key_revoke`` event and
   appends the record to the eventlog.
3. For every archived upatch the revoked key signed, shirudo unloads it
   when it is loaded, with a measured ``upatch_unload`` event whose
   reason is ``key_revoke``, and removes it from the archive. An archived
   upatch that is not loaded is removed without an event.

At a later boot with the same config, the revocation is not a separate
event. Init enrolls the runtime primary in any case, because the boot
needs it until the config is active, and unlinks it again when the
config revokes it. It removes the archived upatches the revoked keys
signed before it restores the archive. The init event covers the set
after revocation, so ``shirudo attest predict`` takes the remaining
certificates only. :doc:`/operate/attest` shows how to predict the
register, and ``--event=revoke:<label>=<cert-sha256>`` adds a revocation
made after boot.

Limits
------

These limits follow from the code as it is. :doc:`/security/threat-model`
discusses the residual risks.

- A signer that the first config did not enroll cannot be added to an
  initialized target. ``image update`` refuses the config with
  ``image: BPF signer was not enrolled at boot; reboot with the new
  trusted key set required``, and a reboot does not select it either. No
  supported procedure resets the activation record. Enroll standby
  signers at first provisioning.
- A config signed by a new config verification key cannot be activated
  on an initialized target. Live activation verifies it under the
  current ``.dm-verity`` keyring, and a new runtime image fails at boot
  as the table above shows.
- Tree publications verify only against the runtime primary
  certificate. Revoking the primary in the config does not stop the
  target from accepting tree publications that the primary signed, and a
  vouched signer cannot sign them. Moving tree publishing to another key
  takes a new runtime image.
- A vouch carries no time. A vouch made by a key that was later revoked
  still admits the certificate it vouches for. The config signer decides
  which certificates are under ``keys/trusted``, so after a compromise,
  revoke the certificates that only the compromised key vouched for as
  well.
- A certificate's validity dates do not end its trust. shirudo verifies
  vouches and index and tree signatures without checking them.
- Any key the kernel trusts for dm-verity signs a valid config. A config
  can change repositories, the tree subscription, export destinations and
  revocations.

Next steps
----------

- :doc:`/operate/attest` to enroll a target and verify evidence.
- :doc:`/operate/deliver-packages` for repositories and their keys.
- :doc:`/reference/cli/keys` and :doc:`/reference/cli/image` for the
  command options.
- :doc:`/security/cryptography` for the algorithms.
