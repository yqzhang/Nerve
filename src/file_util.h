/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __FILE_UTIL__
#define __FILE_UTIL__

#include "pmu_sample.h"
#include "proc_sample.h"

#include <stdbool.h>

void read_file(char* filename, char* read_buffer, unsigned int buffer_size);

void write_file(char* filename, char* write_buffer, unsigned int size,
                bool append);

void write_all(char* filename, bool append, int num_of_cores,
               int num_of_processes, int num_of_events,
               long long irq_info[MAX_NUM_CORES],
               unsigned long long network_info[8],
               unsigned int frequency_info[MAX_NUM_CORES],
               process_external_t* proc_info,
               unsigned long long pmu_info[MAX_NUM_PROCESSES][MAX_EVENTS]);

#endif
