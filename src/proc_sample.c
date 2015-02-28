/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "proc_sample.h"

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

void print_process_info(process_list_t* process_list) {
  int i;
  for (i = 0; i < process_list->size; i++) {
    printf("%d - PID: %d\n", i, process_list->processes[i].process_id);
  }
}

void get_process_info(process_list_t* process_list) {
  DIR* dir_ptr;
  struct dirent* curr_dir_ptr;

  dir_ptr = opendir("/proc/");
  if (dir_ptr == NULL) {
    perror("Cound not open directory /proc/.");
    return;
  }

  process_list->size = 0;
  while ((curr_dir_ptr = readdir(dir_ptr))) {
    if (curr_dir_ptr->d_name[0] >= '0' && curr_dir_ptr->d_name[0] <= '9') {
      if (process_list->size >= MAX_NUM_PROCESSES) {
        perror("Too many processes.");
        return;
      }
      process_list->processes[process_list->size++].process_id =
          atoi(curr_dir_ptr->d_name);
    }
  }

  (void)closedir(dir_ptr);
}
