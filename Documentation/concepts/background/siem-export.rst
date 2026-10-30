..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Explanation
:audience: Operators, SecOps

SIEM export
===========

A target reports its own state and what its upatches observe as
OpenTelemetry records, which :doc:`opentelemetry` explains. This page
follows those records from the target into a SIEM or another backend,
shows where their fields land, and explains which detections they
support and why each one is built as it is.

What a SIEM does
----------------

A security information and event management system, a SIEM, collects
events from many sources into one store: hosts, network devices,
identity providers and applications. It maps them to common fields so
that one search spans every source, keeps them for investigation and
compliance, and runs rules that correlate events and raise alerts.
Splunk and Elastic Security are SIEMs.

Other backends cover part of that. A log store such as Loki keeps and
searches records, a database such as ClickHouse analyzes them in SQL,
and Grafana charts and alerts on both. The same records serve each of
them; what differs is the query language and what the backend charges
for or limits.

From target to backend
----------------------

A target pushes OTLP over HTTP with JSON bodies, through TLS 1.3 with a
hybrid post-quantum key exchange, and nothing else. A SIEM has an
ingest interface of its own, such as Splunk's HTTP Event Collector or
Elasticsearch's bulk API. An OpenTelemetry Collector sits between them.
It accepts the targets' requests, speaks the backend's protocol over
ordinary TLS, batches and retries, and holds the backend's credentials,
so a target carries only what it needs to reach the collector.

The target keeps no spool. A map export that fails loses what the map
collected in that interval, and a keepalive that fails leaves a gap
until the next one. The collector's sending queue is the only buffer on
the way. A collector can also filter or rewrite records before export,
for example to drop the kernel configuration from keepalives, as
:doc:`/operate/run/telemetry` shows.

Where the fields land
---------------------

Every record carries the resource attribute ``host.name``, which names
the target. The event name tells a keepalive, ``shirudo.keepalive``,
from a map snapshot, ``shirudo.bpf.map.snapshot``. The record's
attributes hold the few fields worth filtering and alerting on, among
them ``shirudo.status`` and ``shirudo.tamper_attempts`` on a keepalive.
The body holds the whole document: the ``shirudo status --json`` report,
or what a map's decoder returned.

Each exporter keeps that split in its own way:

.. list-table::
   :header-rows: 1
   :widths: 19 27 29 25

   * - Backend
     - Target
     - Verdict
     - Body
   * - ClickHouse
     - ``ResourceAttributes['host.name']``
     - ``LogAttributes['shirudo.status']``
     - ``Body``, as JSON text
   * - Elasticsearch
     - ``host.name``
     - ``shirudo.status``
     - ``body.structured``
   * - Loki
     - Label ``host_name``
     - Structured metadata ``shirudo_status``
     - Log line
   * - Splunk
     - ``host``
     - Indexed field ``shirudo.status``
     - Event

Loki makes ``host_name`` a label only when configured to, as
:doc:`/operate/integrations/grafana` shows. Rules work best on the
attributes, which every backend keeps as fields of their own, while the
body is large, stored as text by some backends and refused above a size
limit by others.

What to detect
--------------

Three detections cover the keepalive, and
:doc:`/operate/integrations/index` gives each of them for every backend.

A target that goes silent is the first. A target that is down, a timer
that was stopped, a network that fails and an attacker who stopped the
export look the same from the backend: nothing arrives. A rule cannot
match an event that does not exist, so the query compares the last
keepalive of every target it knows against the current time. It knows
the targets that reported within a longer window, such as a day, so a
target silent for longer drops out of the alert. A
keepalive runs five seconds after the previous run ends and may wait up
to 30 seconds for the config operation lock, so two minutes without one
spans several missed intervals.

A verdict other than ``ok`` is the second. The keepalive's severity
follows the verdict, ``WARN`` or ``ERROR``, so a rule on severity
catches the same records.

A rise of ``shirudo.tamper_attempts`` is the third. The attribute counts
the refusals of the guards since boot, so a rise means something on the
target tried an operation the guards refused. The counters start at
zero each boot. A rule that took the largest count in a window minus the
smallest would fire on every reboot. The examples subtract the earliest
count from the latest instead, which a reboot makes negative. The price
is that a rise close to a reboot can go unreported; the count in each
keepalive still shows it.

Map snapshots carry what upatches observe, such as the programs that
``kernel-obs`` saw run. Their fields depend on the upatch's decoder, so
rules on them are written per upatch. The host name and the time of a
record join it to events from other sources.

Volume
------

The keepalive's body includes the system section, and with it the
kernel configuration, so a keepalive from a distribution kernel is a
few hundred kilobytes. At one keepalive every five seconds that adds up
to several gigabytes a day for each target. That matters to a SIEM
priced or capped by ingested volume, and to backends that limit the size
of a record or the rate of a stream. Dropping the kernel configuration
at the collector cuts a keepalive to a few kilobytes, and
``kernel_release`` and ``kernel_build_id`` still name the kernel.

Limits
------

Telemetry is the target's own account of itself. It is not signed and
not bound to the TPM, and root on a target can stop it or send a
keepalive that reports ``ok`` with no refusals. A SIEM alert on a
keepalive tells you where to look. The silence alert catches a target
whose export was stopped, but not one whose root sends forged
keepalives instead.

Evidence of what a target runs is the TPM-signed record that ``shirudo
attest`` produces and ``shirudo attest verify`` checks on the operator
host, described in :doc:`/operate/run/attest`.

Related
-------

- :doc:`opentelemetry` for the records and the protocol
- :doc:`/operate/run/telemetry` to set up export on a target
- :doc:`/operate/integrations/index` for the backend examples
- :doc:`/reference/formats/otlp` for every field a target sends
