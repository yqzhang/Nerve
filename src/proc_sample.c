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

process_info_node_t* get_process_info() {
  DIR* dir_ptr;
  struct dirent* curr_dir_ptr;;

  process_info_node_t* curr_ptr;
  process_info_node_t* head_ptr;
  head_ptr = (process_info_node_t *) malloc(sizeof(process_info_node_t));
  head_ptr->process_id = 0;
  head_ptr->next = NULL;

  dir_ptr = opendir ("/proc/");
  if (dir_ptr == NULL) {
    perror("Cound not open directory /proc/.");
    return NULL;
  }

  while ((curr_dir_ptr = readdir(dir_ptr))) {   
    if(curr_dir_ptr->d_name[0] >= '0' && curr_dir_ptr->d_name[0] <= '9') {
      curr_ptr = (process_info_node_t *) malloc(sizeof(process_info_node_t));
      curr_ptr->process_id = atoi(curr_dir_ptr->d_name);
      curr_ptr->next = head_ptr->next;
      head_ptr->next = curr_ptr;
    }
  }
                   
  (void) closedir(dir_ptr);

  return head_ptr;
}
