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
#include <string.h>
#include <stdbool.h> 

void print_process_info(process_list_t* process_list) {
  int i;
  for (i = 0; i < process_list->size; i++) {
    printf("%d - PID: %d\n", i, process_list->processes[i].process_id);
  }
}

void get_process_info(process_list_t* process_list, int num_iterations) {
  DIR* dir_ptr;
  FILE *fp;
  struct dirent* curr_dir_ptr;
  char pid_stat_location[30]; 
  int i;
  int shift_counter;
  int temp_pid;
  bool next_pid=true;
  size_t size_previous_iteration;
  char check_if_zombie;

  long unsigned int cpu_total_time_after;
  long unsigned int temp_cpu_total_time[7];
  long unsigned int utime_ticks_after;
  long unsigned int stime_ticks_after;
  long unsigned int cutime_ticks_after;
  long unsigned int cstime_ticks_after;

  strcpy(pid_stat_location,"/proc");
  strcat(pid_stat_location, "/stat");
  fp = fopen(pid_stat_location, "r");
  if(fp == NULL){
    perror("Stat File for PID does not exist \n");
  }
  printf("num_iterations %d \n", num_iterations);
  
  fscanf(fp, "%*s %lu %lu %lu %lu %lu %lu %lu",&temp_cpu_total_time[0], 
  &temp_cpu_total_time[1], &temp_cpu_total_time[2], &temp_cpu_total_time[3], 
  &temp_cpu_total_time[4], &temp_cpu_total_time[5], &temp_cpu_total_time[6]);
  fclose(fp);
  
  cpu_total_time_after=0;
  for(i=0; i<7; i++)
    cpu_total_time_after = cpu_total_time_after + temp_cpu_total_time[i];  

  dir_ptr = opendir("/proc/");
  if (dir_ptr == NULL) {
    perror("Cound not open directory /proc/.");
    return;
  }
  size_previous_iteration = process_list->size;
  process_list->size = 0;
  while (true) {
    if(next_pid) {
      curr_dir_ptr = readdir(dir_ptr);
      if(curr_dir_ptr == NULL)
        break;
    }
    if (curr_dir_ptr->d_name[0] >= '0' && curr_dir_ptr->d_name[0] <= '9') {
      memset(&pid_stat_location[0], 0, sizeof(pid_stat_location));
      if (process_list->size >= MAX_NUM_PROCESSES) {
        perror("Too many processes.");
        return;
      }
      
      strcpy(pid_stat_location,"/proc/");
      strcat(pid_stat_location, curr_dir_ptr->d_name);
      strcat(pid_stat_location, "/stat");

      fp = fopen(pid_stat_location, "r");
      if(fp == NULL){
        perror("Stat File for PID does not exist \n");
      }
      fscanf(fp, "%*d %*s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu"
        "%lu %lu", &check_if_zombie, &utime_ticks_after, &stime_ticks_after, 
        &cutime_ticks_after, &cstime_ticks_after);
      fclose(fp);
      //printf("%c\n", check_if_zombie);
      if(check_if_zombie == 'Z') {
        if(num_iterations == 1) { 
          // During the first iteration profile for all values
          process_list->processes[process_list->size].process_id =
              atoi(curr_dir_ptr->d_name);
          process_list->processes[process_list->size].cpu_utilization =
              ( 100 * (float)  (utime_ticks_after + cutime_ticks_after - 
              process_list->processes[process_list->size].user_time - 
              process_list->processes[process_list->size].cuser_time  ) 
              / (cpu_total_time_after - process_list->cpu_total_time)) + 
              ( 100 * (float)  (stime_ticks_after + cstime_ticks_after - 
              process_list->processes[process_list->size].system_time - 
              process_list->processes[process_list->size].csystem_time  ) 
              / (cpu_total_time_after - process_list->cpu_total_time));

          process_list->processes[process_list->size].user_time =
            utime_ticks_after;
          process_list->processes[process_list->size].system_time =
            stime_ticks_after;
          process_list->processes[process_list->size].cuser_time =
            cutime_ticks_after;
          process_list->processes[process_list->size].csystem_time =
            cstime_ticks_after;
          if(process_list->processes[process_list->size].cpu_utilization > 1) {
             printf("PID of that process is %d and utilization is %f\n", 
               process_list->processes[process_list->size].process_id, 
               process_list->processes[process_list->size].cpu_utilization );
          }
          process_list->size++;
        } else {
          //Non first iterations
          temp_pid = atoi(curr_dir_ptr->d_name);
          if(temp_pid == process_list->processes[process_list->size].process_id || process_list->processes[process_list->size].process_id == 0 ) {
          //If the pid at that entry in the queue has not changed!!
          process_list->processes[process_list->size].cpu_utilization =
              ( 100 * (float)  (utime_ticks_after + cutime_ticks_after - 
              process_list->processes[process_list->size].user_time - 
              process_list->processes[process_list->size].cuser_time  ) 
              / (cpu_total_time_after - process_list->cpu_total_time)) + 
              ( 100 * (float)  (stime_ticks_after + cstime_ticks_after - 
              process_list->processes[process_list->size].system_time - 
              process_list->processes[process_list->size].csystem_time  ) 
              / (cpu_total_time_after - process_list->cpu_total_time));
            process_list->processes[process_list->size].user_time =
              utime_ticks_after;
            process_list->processes[process_list->size].system_time =
              stime_ticks_after;
            process_list->processes[process_list->size].cuser_time =
              cutime_ticks_after;
            process_list->processes[process_list->size].csystem_time =
              cstime_ticks_after;

            if(process_list->processes[process_list->size].cpu_utilization > 1)
               printf("PID of that process is %d and utilization is %f\n", 
               process_list->processes[process_list->size].process_id, 
               process_list->processes[process_list->size].cpu_utilization );
            process_list->size++;
            next_pid = true;
            
          } else if (temp_pid < process_list->processes[process_list->size].process_id ) {
            //New PID has been included
              for(shift_counter = size_previous_iteration; shift_counter >= process_list->size; shift_counter--) {
                process_list->processes[shift_counter + 1] = 
                   process_list->processes[shift_counter];
              }
            process_list->processes[process_list->size].process_id =
              temp_pid;
            process_list->processes[process_list->size].user_time =
              utime_ticks_after;
            process_list->processes[process_list->size].system_time =
              stime_ticks_after;
            process_list->processes[process_list->size].cuser_time =
              cutime_ticks_after;
            process_list->processes[process_list->size].csystem_time =
              cstime_ticks_after;
             
            next_pid = true;
            process_list->size++;
          } else if (temp_pid > process_list->processes[process_list->size].process_id ) {
            //Old PID has been deleted
              for(shift_counter = process_list->size; shift_counter < size_previous_iteration; ++shift_counter){
                process_list->processes[shift_counter] = 
                  process_list->processes[shift_counter + 1];
            }
                process_list->processes[size_previous_iteration].process_id = 0;
            next_pid = false;
          } else {
            printf("Something wrong happened \n");
            exit(0);
            //It should technically not come here. Have been tested and seems to be fine.
          }

        }// else loop for iterations after the first one
      } // loop to check if it is zombie
    }// if loop for directory pids
  } // main while loop
  process_list->cpu_total_time = cpu_total_time_after;

  (void)closedir(dir_ptr);
}
