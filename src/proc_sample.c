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

#include "log_util.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void get_process_info(process_list_t* process_list,
                      process_list_t* prev_process_list) {
  struct dirent* curr_dir_ptr;

  // Read /proc/stat for total CPU time spent by far
  // Read /proc/status for context switches
  // cpu %user %nice %system %idle %iowait %irq %softirq
  char pid_stat_location[30] = "/proc/stat";
  char pid_status_location[30];
  FILE *fp;
  FILE *fp2;
  fp = fopen(pid_stat_location, "r");
  if (fp == NULL) {
    logging(LOG_CODE_FATAL, "Stat File %s does not exist.\n", pid_stat_location);
  }


  unsigned long temp_cpu_total_time[7];
  fscanf(fp, "%*s %lu %lu %lu %lu %lu %lu %lu",
         &temp_cpu_total_time[0], &temp_cpu_total_time[1],
         &temp_cpu_total_time[2], &temp_cpu_total_time[3], 
         &temp_cpu_total_time[4], &temp_cpu_total_time[5],
         &temp_cpu_total_time[6]);
  fclose(fp);
  // Reset the size of the process list to 0
  process_list->size = 0;
  // Sum all fields up to get total CPU time
  process_list->cpu_total_time = 0;
  int i;
  for (i = 0; i < 7; i++) {
    process_list->cpu_total_time += temp_cpu_total_time[i];
  }

  // Read /proc/*/stat to get CPU utilization for specific
  // processes
  DIR* dir_ptr = opendir("/proc/");
  if (dir_ptr == NULL) {
    logging(LOG_CODE_FATAL, "Cound not open directory /proc/.");
  }

  int temp_pid;
  i = 0;
  while ((curr_dir_ptr = readdir(dir_ptr)) != NULL) {
    if (curr_dir_ptr->d_name[0] >= '0' && curr_dir_ptr->d_name[0] <= '9') {
      if (process_list->size >= MAX_NUM_PROCESSES) {
        logging(LOG_CODE_FATAL, "Too many processes (max: %d).",
                MAX_NUM_PROCESSES);
      }
      sprintf(pid_stat_location, "/proc/%s/stat", curr_dir_ptr->d_name);
      fp = fopen(pid_stat_location, "r");
      if (fp == NULL) {
        // This means the process has gone shortly after we list the directory
        continue;
      }
      sprintf(pid_status_location, "/proc/%s/status", curr_dir_ptr->d_name);
      fp2 = fopen(pid_status_location, "r");
      if (fp2 == NULL) {
        // This means the process has gone shortly after we list the directory
        continue;
      }

      // Read /proc/*/stat for the CPU and memory utilization information of
      // specific PIDs
      // file format: http://man7.org/linux/man-pages/man5/proc.5.html
      // ...
      // (3)  state  %c  : indicates process state
      // ...
      // (10) minorfault %lu   : minor page faults
      // (11) cminorfault %lu  : child minor page fault
      // (12) majorfault %ld   : major page faults 
      // (13) cmajorfault  %ld : child major page fault
      // (14) utime  %lu       : time spent in user mode
      // (15) stime  %lu       : time spent in kernel mode
      // (16) cutime %ld       : time spent waiting for children in user mode
      // (17) cstime %ld       : time spent waiting for children in kernel mode
      // ...
      // (23) vsize  %lu       : virtual memory size in bytes
      // (24) rss    %ld       : number of pages that the process has in main memory 
      // ...
      char process_state;
      unsigned long utime_ticks, stime_ticks, vsize_bytes;
      unsigned long minor_fault,cminor_fault,major_fault,cmajor_fault;
      long cutime_ticks, cstime_ticks, rss_pages;
      char status_value[256], status_value_prev[256], status_value_before_prev[256];
      
      fscanf(fp,
             "%*d %*s %c %*d %*d %*d %*d %*d %*u %lu %lu %lu %lu " // 1-13
             "%lu %lu %ld %ld %*d %*d %*d %*d %*u %lu %ld", // 14-24
             &process_state, &minor_fault, &cminor_fault, &major_fault, 
             &cmajor_fault,&utime_ticks, &stime_ticks, &cutime_ticks,
             &cstime_ticks, &vsize_bytes, &rss_pages);
      while(fgets(status_value,256, fp2)!= NULL) {
        //value_before_prev = atoi(value_prev);
        strcpy(status_value_before_prev,&status_value_prev[25]);
        strcpy(status_value_prev,status_value);
      }
      strcpy(status_value_prev,&status_value_prev[28]);
      //context_switches = atoi(status_value_prev) + atoi(status_value_before_prev);
      fclose(fp);
      fclose(fp2);

      if (process_state != 'Z') {
        temp_pid = atoi(curr_dir_ptr->d_name);
        process_list->processes[process_list->size].process_id = temp_pid;
        process_list->processes[process_list->size].minor_fault = minor_fault;
        process_list->processes[process_list->size].cminor_fault = cminor_fault;
        process_list->processes[process_list->size].major_fault = major_fault;
        process_list->processes[process_list->size].cmajor_fault = cmajor_fault;
        process_list->processes[process_list->size].total_fault =
            minor_fault + cminor_fault + major_fault + cmajor_fault;
        process_list->processes[process_list->size].utime = utime_ticks;
        process_list->processes[process_list->size].stime = stime_ticks;
        process_list->processes[process_list->size].cutime = cutime_ticks;
        process_list->processes[process_list->size].cstime = cstime_ticks;
        process_list->processes[process_list->size].ttime =
            utime_ticks + stime_ticks + cutime_ticks + cstime_ticks;
        process_list->processes[process_list->size].context_switches =
            atoi(status_value_prev) + atoi(status_value_before_prev);
        process_list->processes[process_list->size].virtual_mem_utilization =
            (float)vsize_bytes / (sysconf(_SC_PHYS_PAGES) *
                                  sysconf(_SC_PAGESIZE));
        process_list->processes[process_list->size].real_mem_utilization =
            (float)rss_pages / sysconf(_SC_PHYS_PAGES);
        //printf("%d %d %s\n",temp_pid,atoi(value_prev), value_before_prev);
        //printf("%d -  %d\n",temp_pid, atoi(value_before_prev));
        //printf("%d -  %d\n",temp_pid, atoi(value_prev));
        //printf("%d -  %s\n",temp_pid, value_before_prev);
        //printf("%d -  %s",temp_pid, value_prev);
        //printf("%d -  %lu\n",temp_pid, process_list->processes[process_list->size].context_switches);

        // Get the CPU affinity information of all child processes/threads
        process_list->processes[process_list->size].cpu_affinity = 0ULL;
        char child_dir_location[64];
        // The chile processes/threads are located in /proc/*/task/
        sprintf(child_dir_location, "/proc/%s/task/", curr_dir_ptr->d_name);
        DIR* child_dir_ptr = opendir(child_dir_location);
        struct dirent* curr_child_dir_ptr;
        while ((curr_child_dir_ptr = readdir(child_dir_ptr)) != NULL) {
          if (curr_child_dir_ptr->d_name[0] >= '0' &&
              curr_child_dir_ptr->d_name[0] <= '9') {
            char child_stat_location[64];
            sprintf(child_stat_location, "/proc/%s/task/%s/stat",
                    curr_dir_ptr->d_name, curr_child_dir_ptr->d_name);
            FILE* child_fp = fopen(child_stat_location, "r");
            if (child_fp == NULL) {
              // This means the it has gone shortly after we list the directory
              continue;
            }
            // Read /proc/*/task/*/stat for the CPU affinity information
            // file format: http://man7.org/linux/man-pages/man5/proc.5.html
            // ...
            // (24) processor  %d : CPU number last executed on
            // ...
            int processor;
            fscanf(child_fp,
                   "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u " // 1 - 12
                   "%*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*d " // 13 - 24
                   "%*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u " // 25 - 36
                   "%*u %*d %d", // 37 - 39
                   &processor);
            fclose(child_fp);
            // Keep a mask for CPU affinity
            process_list->processes[process_list->size].cpu_affinity |=
                (1ULL << processor);
          }
        }
        // Close the directory
        (void)closedir(child_dir_ptr);
	
        // Try to find the PID in the previous list
        while (i < prev_process_list->size &&
               prev_process_list->processes[i].process_id < temp_pid) {
          i++;
        }
        // The PID is not in the original list
        if (i == prev_process_list->size ||
            prev_process_list->processes[i].process_id != temp_pid) {
          process_list->processes[process_list->size].cpu_utilization =
              (float)process_list->processes[process_list->size].ttime /
              (process_list->cpu_total_time -
               prev_process_list->cpu_total_time);
        // The PID is in the original list
        } else {
          process_list->processes[process_list->size].cpu_utilization =
              (float)(process_list->processes[process_list->size].ttime -
                      prev_process_list->processes[i].ttime) /
              (process_list->cpu_total_time -
               prev_process_list->cpu_total_time);
        }
        process_list->size++;
      }
    }
  }

  (void)closedir(dir_ptr);
}

void filter_process_info(process_list_t* process_info_list,
                         process_list_t* filtered_process_info_list,
                         float filter_by_cpu_utilization) {
  filtered_process_info_list->cpu_total_time =
      process_info_list->cpu_total_time;
  filtered_process_info_list->size = 0;
  int i;
  for (i = 0; i < process_info_list->size; i++) {
    if (process_info_list->processes[i].cpu_utilization >=
        filter_by_cpu_utilization) {
      filtered_process_info_list->processes[filtered_process_info_list->size] =
          process_info_list->processes[i];
      filtered_process_info_list->size++;
    }
  }
}

void swap_process_list(process_list_t** process_list_a,
                       process_list_t** process_list_b) {
  process_list_t* temp;
  temp = *process_list_a;
  *process_list_a = *process_list_b;
  *process_list_b = temp;
}

void print_process_info(process_list_t* process_list) {
  int i;
  printf("Size: %zd, CPU total time: %lu\n",
         process_list->size, process_list->cpu_total_time);
  for (i = 0; i < process_list->size; i++) {
    printf("  PID: %u, minor_fault: %lu, cminor_fault: %lu, major_fault: %lu," 
           "cmajor_fault: %lu, utime: %lu, stime: %lu, cutime: %lu, cstime: %lu,"
           "ttime: %lu, cpu_utilization: %f, virtial_mem_utilization: %f, "
           "real_mem_utilization: %f\n",
           process_list->processes[i].process_id,
           process_list->processes[i].minor_fault,
           process_list->processes[i].cminor_fault,
           process_list->processes[i].major_fault,
           process_list->processes[i].cmajor_fault,
           process_list->processes[i].utime,
           process_list->processes[i].stime,
           process_list->processes[i].cutime,
           process_list->processes[i].cstime,
           process_list->processes[i].ttime,
           process_list->processes[i].cpu_utilization,
           process_list->processes[i].virtual_mem_utilization,
           process_list->processes[i].real_mem_utilization);
  }
}
