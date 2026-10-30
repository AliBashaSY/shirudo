..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: How-to
:audience: Operators, SecOps

Export to Splunk
================

Index the keepalives and map snapshots of your targets in Splunk,
through the OpenTelemetry Collector's Splunk HEC exporter, which sends to
Splunk's HTTP Event Collector.

Before you begin
----------------

- Targets that export to a collector, as
  :doc:`/operate/run/telemetry` describes, with the collector's ``otlp``
  receiver set up there.
- The OpenTelemetry Collector Contrib distribution, ``otelcol-contrib``,
  built with Go 1.24 or later. The contrib distribution includes the
  Splunk HEC exporter; Go 1.24 offers the hybrid key exchange the
  targets require.
- An HTTP Event Collector token, an events index for the log records and,
  if a decoder emits metrics, a metrics index.

Set up the exporter
-------------------

#. Add one exporter per index and the pipelines to the collector's
   configuration, next to the ``otlp`` receiver:

   .. code-block:: yaml

      exporters:
        splunk_hec/logs:
          token: ${env:SPLUNK_HEC_TOKEN}
          endpoint: https://<splunk-host>:8088/services/collector
          source: shirudo
          sourcetype: shirudo
          index: shirudo
          tls:
            ca_file: /etc/otelcol/splunk-ca.pem
        splunk_hec/metrics:
          token: ${env:SPLUNK_HEC_TOKEN}
          endpoint: https://<splunk-host>:8088/services/collector
          source: shirudo
          sourcetype: shirudo
          index: shirudo_metrics
          tls:
            ca_file: /etc/otelcol/splunk-ca.pem

      service:
        pipelines:
          logs:
            receivers: [otlp]
            exporters: [splunk_hec/logs]
          metrics:
            receivers: [otlp]
            exporters: [splunk_hec/metrics]

   ``SPLUNK_HEC_TOKEN`` comes from the collector's environment, and
   ``ca_file`` names the CA that signed the HEC endpoint's certificate.
   The exporter takes each event's host from the ``host.name`` resource
   attribute, so every event carries the name of the target that sent
   it.

#. Restart the collector.

Check the data
--------------

#. Search for the events of one target:

   .. code-block:: text

      index=shirudo sourcetype=shirudo host=<target>

   A target sends a keepalive every few seconds, so each target shows up
   within a minute of the restart.

#. List when each target last reported:

   .. code-block:: text

      index=shirudo sourcetype=shirudo
      | stats latest(_time) AS last_seen BY host
      | sort last_seen

   A target at the top whose ``last_seen`` falls behind has stopped
   sending. :doc:`/operate/run/telemetry` describes what else to alert
   on.

Next steps
----------

- :doc:`/reference/formats/otlp` for every field a target sends
- :doc:`/upatches/write/decoders` to turn a map into log records or
  metrics
