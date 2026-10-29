..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: Operators

shirudo keys
============

Show the key material this host holds, and vouch for or add a signing
certificate.

Runs on: any host for the display, the operator host for ``vouch``, and
only a host without an active config image for ``add``. Requires: root on a target,
with the runtime binary. Changes measured state: no.

Usage
-----

.. code-block:: none

   shirudo keys [--keys-dir <dir> | --keys-dir=<dir>]
   shirudo keys add <certificate> (--vouch <vouch> | --vouch=<vouch>)
                    [--keys-dir <dir> | --keys-dir=<dir>]
   shirudo keys vouch <certificate> [-o <vouch>]
                      [--keys-dir <dir> | --keys-dir=<dir>]
                      [--key <signing-key>] [--cert <certificate>]
   shirudo keys revoke
   shirudo keys --list-cmds

``keys`` without a subcommand, or with an option first, shows the key
material. ``shirudo keys --help`` advertises ``keys revoke <fingerprint>
[--promote <fp>]``, but the parser takes no argument and always refuses.

Display
-------

``shirudo keys`` prints what is present:

- the certificate ``cert.der`` and the signing key ``signing_key.pem`` in
  the keys directory: subject, issuer, algorithm, both fingerprints and
  validity.
- the trusted set, when it has more than the primary certificate or
  holds revoked or unvouched entries: each member's SPKI fingerprint, the
  SHA-256 of its certificate and the certificate that vouched for it.
- the ``.bpf`` keyring ``init`` froze: its serial, the frozen serial and
  the keys in it.

The keys directory is ``--keys-dir``, else ``$SHIRUDO_KEYS_DIR``, else
``/var/lib/shirudo/keys``. On a target with an active config image it is
always the runtime image's ``/run/shirudo/runtime/keys``, and the image
gate refuses ``--keys-dir``.

keys add
--------

``keys add`` installs a certificate into the trusted set of a keys
directory, so that ``init`` enrolls it at the next boot:

#. It refuses on a host with an active config image, which every
   production target has, with ``target: add vouched BPF signers to the
   signed config selected for boot and reboot to enroll them``. On such a
   target, add vouched signers to the signed config image instead (see
   :doc:`/operate/keys`).
#. It requires an ML-DSA-87 certificate and a trusted set under the keys
   directory, and refuses a certificate listed under ``<dir>/revoked``.
#. The vouch must verify under a certificate already in the set. With the
   default keys directory, that certificate must also be in the ``.bpf``
   keyring this boot.
#. A certificate already in the set is reported and left alone. The set
   holds at most 16 certificates.
#. It installs the certificate and its vouch under ``<dir>/trusted``,
   logs ``keys add: fingerprint=<fp> vouched_by=<fp> path=<path>`` at
   audit priority, and prints the certificate's fingerprint.

The new certificate takes effect when the initrd is rebuilt with it
(``shirudo-gen-initrd``) and the target reboots.

keys vouch
----------

``keys vouch`` executes the ``shirudo-build`` helper, which signs a vouch
for another certificate with this operator's key:

#. It resolves the key and certificate like :doc:`build`.
#. It refuses a certificate that is not ML-DSA-87 and the operator's own
   certificate.
#. It writes the vouch to ``-o``, else ``<certificate>.vouch``, and prints
   the vouched certificate's fingerprint.

keys revoke
-----------

``keys revoke`` takes no argument and always fails with:

.. code-block:: none

   keys revoke: put revocation records in a signed config image, then run `shirudo image update`

A revocation is the revoked certificate, placed in a signed config image as
``keys/revoked/<fingerprint>.der``, where ``<fingerprint>`` is the
lowercase hexadecimal SHA-256 of its public key (SPKI). When :doc:`image` ``update`` activates that image, the target
removes the certificate from the ``.bpf`` keyring, measures a
``key_revoke`` event in the register and unloads every archived upatch the
certificate signed (see :doc:`/operate/keys`).

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 18 22 25 35

   * - Option
     - Argument
     - Default
     - Description
   * - ``--keys-dir``
     - ``<dir>``, next argument or after ``=``
     - ``$SHIRUDO_KEYS_DIR``, else ``/var/lib/shirudo/keys``
     - Display, ``add`` and ``vouch``: the keys directory.
   * - ``--vouch``
     - ``<vouch>``, next argument or after ``=``
     - none, required
     - ``add``: the vouch an enrolled key signed for the certificate.
   * - ``-o``
     - ``<vouch>``
     - ``<certificate>.vouch``
     - ``vouch``: where to write the vouch.
   * - ``--key``
     - ``<signing-key>``
     - from the keys directory
     - ``vouch``: the signing key.
   * - ``--cert``
     - ``<certificate>``
     - from the keys directory
     - ``vouch``: the signing certificate.
   * - ``--list-cmds``
     - none
     -
     - Print ``add``, ``revoke`` and ``vouch``, one per line.

``vouch`` takes ``-o``, ``--key`` and ``--cert`` with the value as the next
argument only.

Exit status
-----------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Status
     - Meaning
   * - 0
     - Display: key material was found and the keyring is consistent.
       ``add``: the certificate is trusted. ``vouch``: the vouch was
       written.
   * - 1
     - Display: no key material on the host, or a certificate or the
       frozen keyring cannot be read. ``add``: refused or failed, including on a host with an
       active config image. ``vouch``: the key is unusable or signing
       failed. ``revoke``: always.
   * - 2
     - An unknown option or subcommand, a missing certificate or
       ``--vouch``, an extra argument, or an unusable keys directory path.
   * - 127
     - ``vouch``: ``shirudo`` cannot execute the helper.

Files and environment
---------------------

- ``<dir>/cert.der``, ``<dir>/signing_key.pem``, ``<dir>/trusted/``,
  ``<dir>/revoked/``.
- ``/run/shirudo/runtime/keys`` on a target with a config image.
- ``SHIRUDO_KEYS_DIR``: the default keys directory, refused by the image
  gate.

Examples
--------

Show the key material on a target:

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

Vouch for a second operator's certificate:

.. code-block:: console

   operator$ shirudo keys vouch other-cert.der -o other-cert.der.vouch --keys-dir ~/shirudo-keys

Related
-------

- :doc:`/operate/keys`
- :doc:`genkey`
- :doc:`image`
- :doc:`/security/cryptography`
