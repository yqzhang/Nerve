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
  int process_id;
} process_t;

typedef struct process_list {
  process_t processes[MAX_NUM_PROCESSES];
  size_t size;
} process_list_t;

void get_process_info(process_list_t* process_info_list);

void print_process_info(process_list_t* process_info_list);

#endif
