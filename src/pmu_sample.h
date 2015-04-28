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

#define MAX_EVENTS 32

#define MAX_NUM_CORES 40

typedef struct hardware_info {
  int num_of_cores;
  int num_of_events;
  long long irq_info[MAX_NUM_CORES];
  unsigned long long network_info[8];
  unsigned int frequency_info[MAX_NUM_CORES];
  unsigned long long pmu_info[MAX_NUM_PROCESSES][MAX_EVENTS];
} hardware_info_t;

void init_pmu_sample(hardware_info_t* hardware_info);

void get_pmu_sample(process_list_t* process_info_list,
                    const char* events[MAX_EVENTS],
                    unsigned int sample_interval,
                    hardware_info_t* hardware_info);

void clean_pmu_sample();

void record_pmu_sample(
         perf_event_desc_t** fds, int num_fds, int num_pmus,
         unsigned long long pmu_info[MAX_NUM_PROCESSES][MAX_EVENTS],
         int child_thread_mapping[MAX_NUM_PROCESSES * MAX_NUM_THREADS]);

#endif
