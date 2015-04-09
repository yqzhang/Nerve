/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __PMU_SAMPLE_H__
#define __PMU_SAMPLE_H__

#include "proc_sample.h"

#include "perf_util.h"

#include <perfmon/pfmlib_perf_event.h>

#define MAX_GROUPS 256

void init_pmu_sample();

void get_cpu_cycles(unsigned long long* cycles, struct timeval* tvs);

void estimate_frequency();

void get_pmu_sample(process_list_t* process_info_list,
                    const char* events[MAX_GROUPS],
                    unsigned int sample_interval);

void read_groups(perf_event_desc_t* fds, int num);

void print_pmu_sample(perf_event_desc_t** fds, int num_fds, int proc_num,
                      uint64_t proc_info[][20]);

#endif
