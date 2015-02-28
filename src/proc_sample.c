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
#define PID_ARRAY_SIZE 500

void print_pid_list(process_info_node_t pid_list []) {
  int iterator_pid_list;
  for(iterator_pid_list = 0; iterator_pid_list<PID_ARRAY_SIZE; ++iterator_pid_list) {
	if(pid_list[iterator_pid_list].process_id !=0 ) {
		printf("%d - %d\n",iterator_pid_list,pid_list[iterator_pid_list].process_id);
	}
  }

}

process_info_node_t * get_process_info() {
  DIR* dir_ptr;
  struct dirent* curr_dir_ptr;

  process_info_node_t * pid_list = (process_info_node_t *)malloc(PID_ARRAY_SIZE*sizeof(process_info_node_t));
  int iterator_pid_list=-1;

  dir_ptr = opendir ("/proc/");
  if (dir_ptr == NULL) {
    perror("Cound not open directory /proc/.");
    return NULL;
  }

  while ((curr_dir_ptr = readdir(dir_ptr))) {   
    if(curr_dir_ptr->d_name[0] >= '0' && curr_dir_ptr->d_name[0] <= '9') {
      pid_list[++iterator_pid_list].process_id = atoi(curr_dir_ptr->d_name);
    }
  }
                   
  (void) closedir(dir_ptr);

  return pid_list;
}
