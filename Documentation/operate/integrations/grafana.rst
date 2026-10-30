..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: How-to
:audience: Operators, SecOps

Export to Grafana
=================

Show the keepalives, map snapshots and decoder metrics of your targets in
Grafana, with the log records in Loki and the metrics in Prometheus. Both
accept OTLP directly, so the collector forwards to them with its
``otlphttp`` exporter.

Before you begin
----------------

- Targets that export to a collector, as
  :doc:`/operate/run/telemetry` describes, with the collector's ``otlp``
  receiver set up there.
- An OpenTelemetry Collector built with Go 1.24 or later, which offers
  the hybrid key exchange the targets require.
- Loki 3 or later, which stores OTLP attributes as structured metadata.
- Prometheus 3 or later, for the metrics that decoders emit.
- Grafana with Loki and Prometheus as data sources.

Set up Loki and Prometheus
--------------------------

#. In Loki's configuration, keep structured metadata on. Loki 3 enables
   it by default; an older configuration may turn it off:

   .. code-block:: yaml

      limits_config:
        allow_structured_metadata: true

#. Start Prometheus with ``--web.enable-otlp-receiver``, and in
   ``prometheus.yml`` promote the target's host name to a label and accept
   samples that arrive late:

   .. code-block:: yaml

      otlp:
        promote_resource_attributes:
          - host.name
      storage:
        tsdb:
          out_of_order_time_window: 30m

Set up the collector
--------------------

#. Add the exporters and the pipelines to the collector's configuration,
   next to the ``otlp`` receiver:

   .. code-block:: yaml

      exporters:
        otlphttp/loki:
          endpoint: https://<loki-host>/otlp
        otlphttp/prometheus:
          endpoint: https://<prometheus-host>/api/v1/otlp

      service:
        pipelines:
          logs:
            receivers: [otlp]
            exporters: [otlphttp/loki]
          metrics:
            receivers: [otlp]
            exporters: [otlphttp/prometheus]

   The exporter appends ``/v1/logs`` and ``/v1/metrics`` to each
   endpoint.

#. Restart the collector.

Check the data
--------------

#. In Grafana's Explore view with the Loki data source, list the
   keepalives that do not report ``ok``:

   .. code-block:: text

      {service_name="shirudo"} | shirudo_status != "" | shirudo_status != "ok"

   Loki turns ``service.name`` into the label ``service_name`` and stores
   the other attributes as structured metadata, with dots turned into
   underscores: ``shirudo.status`` becomes ``shirudo_status`` and
   ``host.name`` becomes ``host_name``. Only keepalives carry
   ``shirudo.status``.

#. Read the map snapshots of one upatch:

   .. code-block:: text

      {service_name="shirudo"} | shirudo_upatch_name = "<upatch>"

#. With the Prometheus data source, select the metrics of one target:

   .. code-block:: text

      {host_name="<target>"}

   Which metrics exist depends on the decoders of the loaded upatches.

Next steps
----------

- :doc:`/reference/formats/otlp` for every field a target sends
- :doc:`/upatches/write/decoders` to turn a map into log records or
  metrics
- :doc:`clickhouse` to keep the records for longer analysis
