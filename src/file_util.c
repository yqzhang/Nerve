/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "file_util.h"

#include "log_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void read_file(char* filename, char* read_buffer, unsigned int buffer_size) {
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
    logging(LOG_CODE_FATAL, "Error opening file %s.\n", filename);
  }

  fread(read_buffer, buffer_size, 1, fp);
  fclose(fp);
}

void write_file(char* filename, char* write_buffer, unsigned int size,
                bool append) {
  if (size == 0) {
    size = strlen(write_buffer);
  }

  FILE* fp;
  if (append == true) {
    fp = fopen(filename, "a");
  } else {
    fp = fopen(filename, "w");
  }

  if (fp == NULL) {
    logging(LOG_CODE_FATAL, "Error openning file %s.\n", filename);
  }

  fwrite(write_buffer, sizeof(char), size, fp);
  fclose(fp);
}

void write_all(char* filename, bool append, int num_of_cores,
               int num_of_processes, int num_of_events,
               long long irq_info[MAX_NUM_CORES],
               unsigned long long network_info[8],
               unsigned int frequency_info[MAX_NUM_CORES],
               process_external_t* proc_info,
               unsigned long long pmu_info[MAX_NUM_PROCESSES][MAX_EVENTS]) {
  FILE* fp;
  if (append == true) {
    fp = fopen(filename, "a");
  } else {
    fp = fopen(filename, "w");
  }

  if (fp == NULL) {
    logging(LOG_CODE_FATAL, "Error openning file %s.\n", filename);
  }

  /*
   * Write the raw bytes to file in the following format:
   *
   * (1) irq_info        * num_of_cores
   * (2) network_info    * num_of_cores
   * (3) frequency_info  * num_of_cores
   * (4) proc_info       * num_of_processes
   * (5) pmu_info        * num_of_processes * num_events
   */
  fwrite(irq_info, sizeof(long long), num_of_cores, fp);
  fwrite(network_info, sizeof(unsigned long long), 8, fp);
  fwrite(frequency_info, sizeof(unsigned int), num_of_cores, fp);
  fwrite(proc_info, sizeof(process_external_t), num_of_processes, fp);
  int i;
  for (i = 0; i < num_of_processes; i++) {
    fwrite(pmu_info[i], sizeof(unsigned long long), num_of_events, fp);
  }

  fclose(fp);
}
