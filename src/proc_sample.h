/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __PROC_SAMPLE_H__
#define __PROC_SAMPLE_H__

#include <sys/types.h>

// Max number of processes presented in the OS
#define MAX_NUM_PROCESSES 512

// Max number of threads for each process
#define MAX_NUM_THREADS 64

/*
 * There are 2 types of information here:
 * - intermediate: The intermediate values we use for calculation
 * - external: The derived values that can be used externally
 *
 * The intermediate values are kept here only to calculate the external values,
 * which are what we really care about.
 */

// intermediate:
typedef struct process_intermediate {
  unsigned long minflt;
  unsigned long cminflt;
  unsigned long majflt;
  unsigned long cmajflt;
  unsigned long tflt;
  unsigned long utime;
  unsigned long stime;
  unsigned long cutime;
  unsigned long cstime;
  unsigned long ttime;
  unsigned long long voluntary_ctxt_switches;
  unsigned long long nonvoluntary_ctxt_switches;
  unsigned long long read_bytes;
  unsigned long long write_bytes;
  unsigned int child_thread_ids[MAX_NUM_THREADS];
  unsigned int child_thread_ids_size;
} process_intermediate_t;

// external:
typedef struct process_external {
  unsigned int process_id;
  unsigned long long cpu_affinity;
  float page_fault_rate;
  float cpu_utilization;
  float v_ctxt_switch_rate;
  float nv_ctxt_switch_rate;
  float io_read_rate;
  float io_write_rate;
  float virtual_mem_utilization;
  float real_mem_utilization;
} process_external_t;

typedef struct process_list {
  process_intermediate_t processes_i[MAX_NUM_PROCESSES];
  process_external_t processes_e[MAX_NUM_PROCESSES];
  unsigned long cpu_total_time;
  size_t size;
} process_list_t;

void get_process_info(process_list_t* process_info_list,
                      process_list_t* prev_process_info_list);

void filter_process_info(process_list_t* process_info_list,
                         process_list_t* filtered_process_info_list,
                         int num_of_processes);

void swap_process_list(process_list_t** process_list_a,
                       process_list_t** process_list_b);

void print_process_info(process_list_t* process_info_list);

#endif
