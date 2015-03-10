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

#include <time.h>
#include <perfmon/pfmlib_perf_event.h>
#include "perf_util.h"
#include "proc_sample.h"


#define MAX_GROUPS 256


int get_pid_count(process_list_t* process_info_list);

void read_groups(perf_event_desc_t *fds, int num);

void print_counts(perf_event_desc_t **fds, int num_fds, int proc_num, uint64_t proc_info[][20]);

#endif
