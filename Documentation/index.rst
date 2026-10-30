..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

shirudo documentation
=====================

.. rst-class:: etymology

shirudo (Japanese シールド, pronounced "shee-roo-doh") means "shield"

shirudo is a BPF-based security platform: Linux targets run only the BPF
their operator signed, every change is measured into the TPM, and the
functionality ships as signed upatches that targets pull from OCI
registries.

Use cases
---------

shirudo is the platform. It does not need to understand what an upatch
does; it makes sure that only signed upatches run and that every change is
on record.

.. container:: grid

   .. container::

      Live patching for kernel and userspace

      Security mitigations that block vulnerable code paths. They
      complement traditional kernel live patching with a faster rollout
      and bridge the deployment cycles of the kernel and of userspace
      applications.

   .. container::

      System hardening

      Sandboxes for containers, Kubernetes pods and LLM agents, and
      kernel hardening implemented as upatches.

   .. container::

      Runtime security and observability

      For the kernel and userspace applications, LLM agents included.
      Data is collected and aggregated in BPF maps and pushed over
      OpenTelemetry into collectors.

   .. container::

      Minimal overhead

      Agentless, with no daemon and no open port, and built in as part of
      the trusted computing base. Designed for embedded devices as well as
      servers.

Properties
----------

shirudo is built on a set of properties for a BPF security platform:

- Always on: the platform is part of the trusted computing base and starts
  from the initrd in early boot.
- Signed and sealed: BPF is admitted only through loaders signed against
  the sealed BPF keyring, and the platform guards its own assets against
  a root user who does not hold the signing key.
- In-kernel control plane: every load and unload is a signed loader the
  kernel verifies before it runs.
- Measured: a TPM-backed register records the history of every operation
  on upatches, and the TPM signs it for a remote verifier.
- Platform, not policy: shirudo protects only its own assets. Everything
  else ships as upatches: signed BPF light skeletons, a loader and its
  assets.
- Code, not DSLs: upatches are plain BPF C in small pieces that engineers
  and agents can read, generate and maintain, and commands take and
  produce structured data.
- Pull in, push out: targets pull upatches from OCI registries and push
  telemetry to collectors; nothing on a target listens for connections.

The :doc:`security/threat-model` states where each of these mechanisms
stops.

Documentation
-------------

- :doc:`start/index`: build shirudo, boot a protected test target and take
  one upatch through its whole life, then install it.
- :doc:`concepts/index`: the mechanisms shirudo builds on, from BPF signing
  and the BPF LSM to dm-verity, TPM 2.0 and post-quantum cryptography, and
  the vocabulary of the rest of the documentation.
- :doc:`operate/index`: system requirements, deployment, attestation,
  monitoring, upgrades, troubleshooting and incident response for targets
  in production.
- :doc:`upatches/index`: write, test, package and publish upatches.
- :doc:`security/index`: the security model, the threat model and the
  threats still to resolve, cryptography and tamper counters.
- :doc:`internals/index`: how shirudo works inside the kernel and on the
  target, and why it is built this way.
- :doc:`reference/index`: every command, format, setting, event, failure
  category, file and unit, and a cheat sheet.
- :doc:`contribute/index`: work on shirudo itself, its tests and its
  documentation.

.. toctree::
   :hidden:

   start/index
   concepts/index
   operate/index
   upatches/index
   security/index
   internals/index
   reference/index
   contribute/index
