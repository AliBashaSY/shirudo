..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: How-to
:audience: Operators, SecOps

Export to ClickHouse
====================

Store the keepalives and map snapshots of your targets in ClickHouse,
through the OpenTelemetry Collector's ClickHouse exporter. The exporter
writes OTLP logs to one table and each kind of metric to a table of its
own.

This is a do-it-yourself example to get started from, not an
integration that shirudo ships or maintains. Adapt retention, access
control and sizing to your deployment.

Before you begin
----------------

- Targets that export to a collector, as
  :doc:`/operate/run/telemetry` describes, with the collector's ``otlp``
  receiver set up there.
- The OpenTelemetry Collector Contrib distribution, ``otelcol-contrib``,
  built with Go 1.24 or later. The contrib distribution includes the
  ClickHouse exporter; Go 1.24 offers the hybrid key exchange the
  targets require.
- A ClickHouse server the collector can reach, and a user for the
  collector that may create its database and tables and insert into
  them.

The exporter's log support is beta and its metric support alpha.

Set up the exporter
-------------------

#. Create the database and the collector's user:

   .. code-block:: sql

      CREATE DATABASE shirudo;
      CREATE USER otel IDENTIFIED BY '<password>';
      GRANT CREATE DATABASE, CREATE TABLE, INSERT, SELECT ON shirudo.* TO otel;

#. Add the exporter and the pipelines to the collector's configuration,
   next to the ``otlp`` receiver:

   .. code-block:: yaml

      exporters:
        clickhouse:
          endpoint: tcp://<clickhouse-host>:9440?secure=true
          database: shirudo
          username: otel
          password: ${env:CLICKHOUSE_PASSWORD}
          create_schema: true
          ttl: 720h
          sending_queue:
            batch:
              flush_timeout: 5s
              min_size: 5000

      service:
        pipelines:
          logs:
            receivers: [otlp]
            exporters: [clickhouse]
          metrics:
            receivers: [otlp]
            exporters: [clickhouse]

   ``create_schema`` lets the exporter create its tables, ``otel_logs``
   for logs and ``otel_metrics_gauge``, ``otel_metrics_sum`` and their
   siblings for metrics. It also runs ``CREATE DATABASE IF NOT EXISTS``
   at every start, which needs the ``CREATE DATABASE`` grant even though
   the database exists. Set it to ``false`` to create the tables
   yourself.
   ``ttl`` drops rows older than 30 days. ``CLICKHOUSE_PASSWORD`` comes
   from the collector's environment.

#. Restart the collector.

Check the data
--------------

#. List the latest keepalives:

   .. code-block:: sql

      SELECT Timestamp,
             ResourceAttributes['host.name'] AS host,
             LogAttributes['shirudo.status'] AS status
      FROM shirudo.otel_logs
      WHERE ServiceName = 'shirudo' AND EventName = 'shirudo.keepalive'
      ORDER BY Timestamp DESC
      LIMIT 10;

   A target sends a keepalive every few seconds, so each target shows up
   within a minute of the restart.

#. Show the last verdict of every target and when it last reported:

   .. code-block:: sql

      SELECT ResourceAttributes['host.name'] AS host,
             argMax(LogAttributes['shirudo.status'], Timestamp) AS status,
             max(Timestamp) AS last_seen
      FROM shirudo.otel_logs
      WHERE ServiceName = 'shirudo' AND EventName = 'shirudo.keepalive'
      GROUP BY host
      ORDER BY last_seen;

   A target at the top whose ``last_seen`` falls behind has stopped
   sending, and a ``status`` other than ``ok`` needs a look, as
   :doc:`/operate/run/telemetry` describes under what to alert on.

#. Read the map snapshots of one upatch:

   .. code-block:: sql

      SELECT Timestamp, LogAttributes['shirudo.map.name'] AS map, Body
      FROM shirudo.otel_logs
      WHERE EventName = 'shirudo.bpf.map.snapshot'
        AND LogAttributes['shirudo.upatch.name'] = '<upatch>'
      ORDER BY Timestamp DESC
      LIMIT 10;

   ``Body`` holds what the map's decoder returned, or the map's entries
   when the upatch ships no decoder. A decoder that emits metrics writes
   to the metric tables instead.

Alert on the data
-----------------

Each query below returns one row for every target that needs a look,
so an alert fires when a query returns rows. Run them on a schedule,
from Grafana with its ClickHouse data source or from any tool that can
run SQL. :doc:`/operate/run/telemetry` explains each signal under what
to alert on.

#. Find targets that stopped sending keepalives. Two minutes covers
   several keepalive intervals and the 30 seconds a keepalive may wait
   for the config operation lock:

   .. code-block:: sql

      SELECT ResourceAttributes['host.name'] AS host,
             max(Timestamp) AS last_seen
      FROM shirudo.otel_logs
      WHERE ServiceName = 'shirudo' AND EventName = 'shirudo.keepalive'
      GROUP BY host
      HAVING last_seen < now() - INTERVAL 2 MINUTE;

#. Find targets that reported a verdict other than ``ok`` in the last
   five minutes:

   .. code-block:: sql

      SELECT ResourceAttributes['host.name'] AS host,
             LogAttributes['shirudo.status'] AS status,
             max(Timestamp) AS last_seen
      FROM shirudo.otel_logs
      WHERE ServiceName = 'shirudo' AND EventName = 'shirudo.keepalive'
        AND Timestamp > now() - INTERVAL 5 MINUTE
        AND LogAttributes['shirudo.status'] != 'ok'
      GROUP BY host, status;

#. Find targets whose guards refused operations in the last ten
   minutes:

   .. code-block:: sql

      SELECT ResourceAttributes['host.name'] AS host,
             argMax(toInt64OrZero(LogAttributes['shirudo.tamper_attempts']), Timestamp)
             - argMin(toInt64OrZero(LogAttributes['shirudo.tamper_attempts']), Timestamp)
               AS new_attempts
      FROM shirudo.otel_logs
      WHERE ServiceName = 'shirudo' AND EventName = 'shirudo.keepalive'
        AND Timestamp > now() - INTERVAL 10 MINUTE
      GROUP BY host
      HAVING new_attempts > 0;

   The query subtracts the earliest count in the window from the latest.
   The counters start at zero each boot, so a target that rebooted in
   the window yields a negative difference and no row, where the
   difference between the largest and smallest count would report every
   reboot.

Next steps
----------

- :doc:`/reference/formats/otlp` for every field a target sends
- :doc:`/upatches/write/decoders` to turn a map into log records or
  metrics
- :doc:`grafana` to chart the same data
- :doc:`/concepts/background/siem-export` for how the records reach a
  backend and what to build on them
