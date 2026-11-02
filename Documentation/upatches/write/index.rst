..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

Write
=====

Where a program can attach, how an upatch keeps and exports its state in
maps, arenas and serializers, how a Lua decoder turns that state into
telemetry, and how to test the memory ordering that programs on several
CPUs rely on.

.. toctree::
   :titlesonly:

   attach-kinds
   maps-arenas-serializers
   decoders
   memory-consistency/index
