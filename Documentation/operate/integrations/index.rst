..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

Integrations
============

Send what targets export into the tools you already run. Targets speak
OTLP over HTTP with JSON bodies and TLS 1.3 with a hybrid post-quantum
key exchange, and nothing else. Each page puts an OpenTelemetry Collector
between the targets and one backend: the collector accepts the targets'
requests, batches and retries, and speaks the backend's own protocol
over ordinary TLS, so the backend needs no post-quantum support.

.. toctree::
   :titlesonly:

   clickhouse
   splunk
   grafana
