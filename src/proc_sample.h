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
/*
Max processes can only be 511
*/

typedef struct process {
  int process_id;
  long unsigned int user_time;
  long unsigned int system_time;
  long unsigned int cuser_time;
  long unsigned int csystem_time;
  float cpu_utilization;
} process_t;

typedef struct process_list {
  process_t processes[MAX_NUM_PROCESSES];
  long unsigned int cpu_total_time;
  size_t size;
} process_list_t;

void get_process_info(process_list_t* process_info_list, int num_iterations);

void print_process_info(process_list_t* process_info_list);

#endif
