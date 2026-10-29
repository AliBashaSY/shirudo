..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: Operators

shirudo image
=============

Build and verify signed dm-verity images on the operator host, and
activate a signed config image on a target.

Runs on: ``build`` and ``verify`` on the operator host, through the
``shirudo-build`` helper, without the image gate. ``update`` and
``status`` on a target, as root with the runtime binary. Requires: ``veritysetup`` on the
operator host. Changes measured state: ``update`` yes, the others no.

Usage
-----

.. code-block:: none

   shirudo image build <filesystem-image> (--output | -o) <prefix>
                       [--kind config|runtime] [--keys-dir <dir>]
                       [--key <signing-key>] [--cert <certificate>]
                       [--salt <salt>]
   shirudo image verify <prefix> --cert <certificate>
   shirudo image update <prefix>
   shirudo image status
   shirudo image --list-cmds

The subcommand names must be spelled out. ``image`` without a subcommand
prints the usage and exits with 2. ``--list-cmds`` prints
``build verify update status`` on one line.

An image is four files that share a prefix: ``<prefix>.img`` (the ext4
file system), ``<prefix>.verity`` (the hash tree), ``<prefix>.roothash``
(64 lowercase hexadecimal characters) and ``<prefix>.p7s`` (the
ML-DSA-87 signature over those 64 characters).

image build
-----------

The helper runs ``shirudo-gen-verity`` (see :doc:`gen-tools`) with the
arguments, which:

#. checks that the input is a regular file, not a symbolic link,
   non-empty, a multiple of 4096 bytes and at most 1 TiB, and that none of
   the four output files exists,
#. copies the input and runs ``veritysetup format`` with SHA-256, 4096-byte
   blocks and, with ``--salt``, the given salt,
#. signs the root hash with ``shirudo-build image sign-root``, verifies
   the result with ``shirudo-build image verify``, and links the four
   files into place,
#. prints the root hash.

Every option takes its value as the next argument. ``--kind`` must be
``config`` or ``runtime`` and changes nothing else. ``image build
--help`` prints the usage of ``shirudo-gen-verity``. To build the file
system from a directory, use ``shirudo-gen-image`` or
``shirudo-gen-runtime`` (:doc:`gen-tools`).

image verify
------------

The helper reads ``<prefix>.roothash`` and ``<prefix>.p7s``, checks that
``--cert`` is an ML-DSA-87 certificate that signed the root hash, runs
``veritysetup verify`` on the data and hash files it opened, and prints the
root hash. The arguments must come in exactly this order:
``verify <prefix> --cert <certificate>``.

image update
------------

``update`` activates a new signed config image on a target. ``<prefix>``
is a local path prefix or an ``https://`` URL prefix.

#. It fetches ``<prefix>.roothash``, ``.p7s``, ``.img`` and ``.verity``
   into a staging directory under ``/var/lib/shirudo/images``, over HTTPS
   with redirects to HTTPS only, or from local regular files.
#. It checks the signature through the kernel's dm-verity trust path,
   maps the image read-only and reads it completely.
#. It links the files to ``/var/lib/shirudo/images/<root-hash>.*``. When
   the root hash is the active one, it stops here.
#. It maps and mounts the candidate, validates its settings, and checks
   that every signer its trusted set names is already in the ``.bpf``
   keyring. A live update cannot enroll a new key.
#. It records the activation as pending, extends the config PCR named in
   the bootstrap settings with an ``UPDATE`` event, checks the PCR value
   and appends the event to the activation records.
#. It applies the revocations in the image: each revoked certificate
   leaves the ``.bpf`` keyring, the register receives a ``key_revoke``
   event, and every archived upatch the certificate signed is unloaded
   and measured. It mounts the image at ``/run/shirudo/config``.
#. It clears the pending record.

When ``update`` stops between steps 5 and 7, the activation stays pending.
Every command except ``status`` is then refused until the next reboot.
shirudo does not reboot the target.

image status
------------

``status`` prints one JSON object with the active selection:
``root_hash``, ``pcr``, ``sequence``, ``reset_count``, ``restart_count``,
``pending`` and ``consistent``.

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 16 22 30 32

   * - Option
     - Argument
     - Default
     - Description
   * - ``-o``, ``--output``
     - ``<prefix>``
     - none, required
     - ``build``: the output prefix.
   * - ``--kind``
     - ``config`` or ``runtime``
     - ``config``
     - ``build``: validated, no other effect.
   * - ``--keys-dir``
     - ``<dir>``
     - ``$SHIRUDO_KEYS_DIR``, else ``/var/lib/shirudo/keys``
     - ``build``: directory of the signing key and certificate.
   * - ``--key``
     - ``<signing-key>``
     - ``<dir>/signing_key.pem``
     - ``build``: the signing key.
   * - ``--cert``
     - ``<certificate>``
     - ``<dir>/cert.der`` for ``build``, required for ``verify``
     - ``build``: the signing certificate. ``verify``: the certificate
       to verify against.
   * - ``--salt``
     - ``<salt>``, 64 hex characters
     - chosen by ``veritysetup``
     - ``build``: the dm-verity salt.

Exit status
-----------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Status
     - Meaning
   * - 0
     - Success. ``status``: the activation is complete and consistent.
   * - 1
     - ``build``: a failure or a usage error reported by
       ``shirudo-gen-verity``. ``verify``: the signature or the hash tree
       does not verify, or a file is missing. ``update``: refused or
       failed, and while pending ``image: activation is pending; further
       changes are blocked``. ``status``: the activation is pending or
       inconsistent, and the JSON is still printed. ``update`` and ``status``
       on a host without config images fail with ``No such device``.
   * - 2
     - No subcommand, an unknown subcommand, a wrong number of arguments
       to ``update`` or ``status``, or ``verify`` arguments in another
       form.
   * - 127
     - ``build``, ``verify``: ``shirudo`` cannot execute the helper.

``shirudo-gen-verity`` stops at the first failing tool, so ``build`` can
also exit with that tool's status, for example ``veritysetup``'s.

Files and environment
---------------------

- ``build``: the four output files and a temporary directory beside
  them. ``SHIRUDO_KEYS_DIR`` and ``SHIRUDO_BUILD`` set the defaults of
  ``shirudo-gen-verity``.
- ``update``: ``/var/lib/shirudo/images``, ``/var/lib/shirudo/config-update``,
  ``/run/shirudo/config``, the config PCR and the register.

Examples
--------

Sign an ext4 image as a config image:

.. code-block:: console

   operator$ shirudo image build config.ext4 --output config --keys-dir ~/shirudo-keys

Verify an image offline:

.. code-block:: console

   operator$ shirudo image verify config --cert config-cert.der

Activate a config image a web server publishes:

.. code-block:: console

   target# shirudo image update https://config.example/shirudo/config

Show the active selection:

.. code-block:: console

   target# shirudo image status
   {"root_hash":"12dbf3c750834908c0f6510481e35fbb0cc76cf6b1896de524304535e63aff0f","pcr":14,"sequence":1,"reset_count":1,"restart_count":0,"pending":false,"consistent":true}

Related
-------

- :doc:`/internals/config-images`
- :doc:`/concepts/background/dm-verity`
- :doc:`gen-tools`
- :doc:`/reference/formats/configuration`
- :doc:`/operate/keys`
