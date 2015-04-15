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

#define MAX_NUM_PROCESSES 512

typedef struct process {
  unsigned int process_id;
  unsigned long minor_fault;
  unsigned long cminor_fault;
  unsigned long major_fault;
  unsigned long cmajor_fault;
  unsigned long long total_fault;
  unsigned long utime;
  unsigned long stime;
  unsigned long cutime;
  unsigned long cstime;
  unsigned long ttime;
  unsigned long long cpu_affinity;
  int io_read;
  int io_write;
  int context_switches;
  float cpu_utilization;
  float virtual_mem_utilization;
  float real_mem_utilization;
} process_t;

typedef struct process_list {
  process_t processes[MAX_NUM_PROCESSES];
  unsigned long cpu_total_time;
  size_t size;
} process_list_t;

void get_process_info(process_list_t* process_info_list,
                      process_list_t* prev_process_info_list);

void filter_process_info(process_list_t* process_info_list,
                         process_list_t* filtered_process_info_list,
                         float filter_by_cpu_utilization);

void swap_process_list(process_list_t** process_list_a,
                       process_list_t** process_list_b);

void print_process_info(process_list_t* process_info_list);

#endif
