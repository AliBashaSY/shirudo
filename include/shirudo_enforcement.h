/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/* Copyright (c) 2026 Authors of shirudo */

#ifndef SHIRUDO_ENFORCEMENT_H
#define SHIRUDO_ENFORCEMENT_H

#define SHIRUDO_PR_GET_ENFORCEMENT 0x53520001
#define SHIRUDO_PR_ENFORCING_ERR 1002

/* PERF_EVENT_IOC_SET_BPF, _IOW('$', 8, __u32), on every supported architecture. */
#define SHIRUDO_PERF_EVENT_IOC_SET_BPF 0x40042408u

#endif /* SHIRUDO_ENFORCEMENT_H */
