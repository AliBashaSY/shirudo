..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Reference
:audience: Operators

shirudo status
==============

Report the local readiness and state of this host, and optionally send the
report as an OTLP keepalive.

Runs on: any host, and on a target as root with the runtime binary. It is the
only command that runs while a config activation is pending or cannot be
verified. Requires: nothing. It runs before ``init`` too. Changes measured
state: no.

Usage
-----

.. code-block:: none

   shirudo status [--json] [--otel] [--include-system] [--include-eventlog]
                  [--include-kernellog | --include-kernellog=<lines>]
                  [--include-modules] [--show-full]

Description
-----------

``status`` reads the state of the host without changing it and prints a
report. The roles come from what is present:

- target: the certificate (:doc:`init` lists its path), the pinned LSM
  link ``/run/shirudo/bpffs/lsm_link`` or the ``.bpf`` keyring that
  ``init`` froze
- operator: ``/var/lib/shirudo/keys/signing_key.pem``

``Mode`` is ``production`` when the LSM link is pinned and ``inactive``
otherwise. It says nothing about the keyring. The ``KeyringSealed``
condition does.

The report shows five conditions, each ``OK``, ``WARN`` or ``ALERT``:
``LsmAttached``, ``KeyringSealed``, ``ChainIntact``, ``TpmMeasured`` and
``ConfigActive``. The verdict is the first of these that applies:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Verdict
     - Applies when
   * - ``tampered``
     - The live register differs from the eventlog replay, the ``.bpf``
       or ``.dm-verity`` keyring is not the one ``init`` froze or is
       gone, a keyring holds a link ``init`` did not freeze, the config
       key is gone from ``.dm-verity``, or the selected config cannot be
       verified against its records.
   * - ``lsm-off``
     - The LSM link is not pinned.
   * - ``init-pending``
     - ``init`` has not frozen a ``.bpf`` keyring this boot.
   * - ``reboot-pending``
     - A config activation is pending until the next reboot.
   * - ``degraded``
     - No TPM answered, or no register is bound this boot.
   * - ``ok``
     - None of the above.

The human report follows the verdict with the tamper counters that moved
this boot, the ``.bpf`` and ``.dm-verity`` keyrings with the
certificates each should hold and the key that holds each one, the
loaded upatches, the TPM measurement state and the assets under
``/var/lib/shirudo``. :doc:`/operate/run/read-status` explains each field and
:doc:`/reference/formats/status-json` the JSON document.

With ``--otel``, ``status`` builds the JSON report and sends it as one
OTLP log record named ``shirudo.keepalive``, only when an endpoint is
configured: ``map_export.otel_endpoint``, else
``OTEL_EXPORTER_OTLP_LOGS_ENDPOINT``, else
``OTEL_EXPORTER_OTLP_ENDPOINT``. Without an endpoint it sends nothing and
says nothing, except while a config activation is pending or unverifiable,
when it prints a warning. ``--otel`` alone prints nothing on standard
output. Add ``--json`` to print the document too.

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 26 14 12 48

   * - Option
     - Argument
     - Default
     - Description
   * - ``--json``
     - none
     - off
     - Print the report as one JSON document.
   * - ``--otel``
     - none
     - off
     - Send the JSON report as an OTLP keepalive log record.
   * - ``--include-system``
     - none
     - off
     - Add a ``system`` section: host, operating system, kernel and its
       build ID, machine, boot ID, CPUs, memory, disk and uptime.
   * - ``--include-eventlog``
     - none
     - off
     - Add the eventlog entries.
   * - ``--include-kernellog``
     - ``<lines>``, 1 to 100, after ``=``
     - 10
     - Add the last lines of the kernel ring buffer.
   * - ``--include-modules``
     - none
     - off
     - Add the 10 most recently loaded kernel modules.
   * - ``--show-full``
     - none
     - off
     - Turn on every ``--include-*`` option and print the module
       dependency tree.

Exit status
-----------

``status`` exits with 0 whatever the verdict, ``tampered`` included.
Monitoring must read ``.status`` and the conditions from the JSON
document.

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Status
     - Meaning
   * - 0
     - The report was produced.
   * - 1
     - With ``--otel``: the configuration cannot be loaded or the OTLP
       request failed. With ``--json``: the document does not fit the
       output buffer. Also an allocation failure.
   * - 2
     - An unknown option, a ``--include-kernellog`` value outside 1 to
       100, or with ``--otel`` an invalid ``map_export`` configuration.

Files and environment
---------------------

- Reads ``/run/shirudo/bpffs`` (pins, identity, tamper counters), the
  ``.bpf`` and ``.dm-verity`` keyrings, the TPM register,
  ``/var/lib/shirudo/eventlog``, ``/var/lib/shirudo/keys``, the config
  certificate the runtime image's bootstrap names and the config
  activation records
- With ``--otel``: ``map_export.*`` and the ``OTEL_EXPORTER_OTLP_*``
  variables (see :doc:`/reference/environment`).
  ``shirudo-keepalive.service`` reads them from
  ``/etc/shirudo/map-export.env``

Examples
--------

Show the state of a target:

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

   Tamper attempts (refused this boot):
     LoadNotLoader            21  program load that is not a signed loader
     MapCreate                 1  map creation by another executable
     BpfCommand               10  detach, update, delete or unclassified bpf() command
     SealPtrace                7  ptrace of a shirudo task

   Keyring (.bpf):
     Serial:                130847582
     Frozen serial:         130847582  (matches)
     Certificates:          2 trusted, 0 revoked, 0 unvouched, 2 in the bpf keyring
       d1bc43c91b9dabf4cd6f3ddf08a177927b3dac8ca833ddc496167253761b4497  (primary)
         CN=shirudo, ML-DSA-87, issued 2026-10-29
         key 199275792  shirudo: 6ec99b4c3d1c51e461ce1299fae6ab87d212ce9c  (frozen)
       c1c30b07f86808e81ae1c0458c926f00b85bcc815d371d23d7683903f2d3266e
         CN=shirudo, ML-DSA-87, issued 2026-10-29
         key 106884895  shirudo: c350bd270c426c5760b202062e15e274ea30f45d  (frozen)

   Keyring (.dm-verity):
     Serial:                962472513
     Frozen serial:         962472513  (matches)
     Certificates:          1 trusted, 1 in the dm-verity keyring
       d1bc43c91b9dabf4cd6f3ddf08a177927b3dac8ca833ddc496167253761b4497  (config)
         CN=shirudo, ML-DSA-87, issued 2026-10-29
         key 200951639  shirudo: 6ec99b4c3d1c51e461ce1299fae6ab87d212ce9c  (frozen)

   upatches loaded (1):
     NAME                      LOCATION                       MAPS  STATUS                        AGE
     shirudo-test-basic        fentry/do_nanosleep               0  success                       2s

   TPM Measurement:
     TPM:               reachable (device:/dev/tpmrm0)
     NV index:          0x01818E00 (anchored in PCR 9)
     Chain:             0307d216caac3067fe645d4a548b25ac5ed0f1e8cf5ae3b189feaefc47e81a84
     Register name:     000b9b7b0f925415fdf0d5676369f8cc9146b9e3142537148b31e79dbffcc96e72f8
     Anchored:          yes (name on record)
     PCR before init:   0000000000000000000000000000000000000000000000000000000000000000
     Phase at init:     0000000000000000000000000000000000000000000000000000000000000000 (PCR 11)
     Bound in:          initrd
     Chain vs eventlog: matches
     Eventlog:          /var/lib/shirudo/eventlog  (2 entries, 2324 bytes)

   Assets (/var/lib/shirudo):
     keys/cert.der          present
     keys/signing_key.pem   absent
     eventlog               present (2324 bytes)
     upatches/              1 .upatch
     incoming/              empty
     bpffs/                 mounted

Read the verdict in a script:

.. code-block:: console

   target# shirudo status --json | jq -r .status
   ok

Add this boot's eventlog to the report:

.. code-block:: console

   target# shirudo status --include-eventlog

The eventlog section of that report:

.. code-block:: none

   Eventlog (audit trail):
     TIME                 TYPE          DIGEST        DETAIL
     2026-10-29T16:17:38Z init          cb206abf8e51  set_digest=e3b0c44298fc..
     2026-10-29T16:21:19Z upatch_load   d979b373d4e5  shirudo-test-basic payload=8aee7072a1a1..
     2026-10-29T16:21:20Z upatch_unload 26dae981e9bb  shirudo-test-basic payload=8aee7072a1a1..

Related
-------

- :doc:`/operate/run/read-status`
- :doc:`/reference/formats/status-json`
- :doc:`/security/tamper-counters`
- :doc:`/operate/troubleshoot/incident-tampered`
- :doc:`/operate/run/telemetry`
