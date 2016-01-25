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

#include <ctype.h>
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
  // cpu %user %nice %system %idle %iowait %irq %softirq
  char pid_stat_location[32] = "/proc/stat";

  FILE *fp;
  fp = fopen(pid_stat_location, "r");
  if (fp == NULL) {
    logging(LOG_CODE_FATAL,
            "Stat File %s does not exist.\n",
            pid_stat_location);
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

      // Read /proc/*/stat for information about:
      // - CPU, memory, page faults
      // file format: http://man7.org/linux/man-pages/man5/proc.5.html
      // ...
      // (3)  state   %c  : indicates process state
      // ...
      // (10) minflt  %lu : minor page faults
      // (11) cminflt %lu : minor page faults by children
      // (12) majflt  %lu : major page faults
      // (13) cmajflt %lu : major page faults by children
      // (14) utime   %lu : time spent in user mode
      // (15) stime   %lu : time spent in kernel mode
      // (16) cutime  %ld : time spent waiting for children in user mode
      // (17) cstime  %ld : time spent waiting for children in kernel mode
      // ...
      // (23) vsize   %lu : virtual memory size in bytes
      // (24) rss     %ld : number of pages that the process has in main memory
      // ...
      char process_state;
      unsigned long minflt, cminflt, majflt, cmajflt;
      unsigned long utime_ticks, stime_ticks, vsize_bytes;
      long cutime_ticks, cstime_ticks, rss_pages;
      fscanf(fp,
             "%*d %*s %c %*d %*d %*d %*d %*d %*u %lu %lu %lu %lu " // 1-13
             "%lu %lu %ld %ld %*d %*d %*d %*d %*u %lu %ld", // 14-24
             &process_state, &minflt, &cminflt, &majflt, &cmajflt,
             &utime_ticks, &stime_ticks, &cutime_ticks, &cstime_ticks,
             &vsize_bytes, &rss_pages);
      fclose(fp);

      if (process_state != 'Z') {
        temp_pid = atoi(curr_dir_ptr->d_name);
        // PID
        process_list->processes_e[process_list->size].process_id = temp_pid;
        // Page faults
        process_list->processes_i[process_list->size].minflt = minflt;
        process_list->processes_i[process_list->size].cminflt = cminflt;
        process_list->processes_i[process_list->size].majflt = majflt;
        process_list->processes_i[process_list->size].cmajflt = cmajflt;
        process_list->processes_i[process_list->size].tflt =
            minflt + cminflt + majflt + cmajflt;
        // CPU utilization
        process_list->processes_i[process_list->size].utime = utime_ticks;
        process_list->processes_i[process_list->size].stime = stime_ticks;
        process_list->processes_i[process_list->size].cutime = cutime_ticks;
        process_list->processes_i[process_list->size].cstime = cstime_ticks;
        process_list->processes_i[process_list->size].ttime =
            utime_ticks + stime_ticks + cutime_ticks + cstime_ticks;
        // Memory usage
        process_list->processes_e[process_list->size].virtual_mem_utilization =
            (float)vsize_bytes / (sysconf(_SC_PHYS_PAGES) *
                                  sysconf(_SC_PAGESIZE));
        process_list->processes_e[process_list->size].real_mem_utilization =
            (float)rss_pages / sysconf(_SC_PHYS_PAGES);

        // Try to find the PID in the previous list
        while (i < prev_process_list->size &&
               prev_process_list->processes_e[i].process_id < temp_pid) {
          i++;
        }
        // The PID is not in the original list
        if (i == prev_process_list->size ||
            prev_process_list->processes_e[i].process_id != temp_pid) {
          // Page fault rate
          process_list->processes_e[process_list->size].page_fault_rate =
              (float)process_list->processes_i[process_list->size].tflt /
              (process_list->cpu_total_time -
               prev_process_list->cpu_total_time);
          // CPU utilization
          process_list->processes_e[process_list->size].cpu_utilization =
              (float)process_list->processes_i[process_list->size].ttime /
              (process_list->cpu_total_time -
               prev_process_list->cpu_total_time);
        // The PID is in the original list
        } else {
          // Page fault rate
          process_list->processes_e[process_list->size].page_fault_rate =
              (float)(process_list->processes_i[process_list->size].tflt -
                      prev_process_list->processes_i[i].tflt) /
              (process_list->cpu_total_time -
               prev_process_list->cpu_total_time);
          // CPU utilization
          process_list->processes_e[process_list->size].cpu_utilization =
              (float)(process_list->processes_i[process_list->size].ttime -
                      prev_process_list->processes_i[i].ttime) /
              (process_list->cpu_total_time -
               prev_process_list->cpu_total_time);
        }
        process_list->size++;
      }
    }
  }

  (void)closedir(dir_ptr);
}

void get_process_stats(process_list_t* filtered_process_list,
                       process_list_t* process_list,
                       process_list_t* prev_process_list) {
  int proc_index;
  char pid_io_location[32];
  char pid_status_location[32];
  FILE *fp;

  // Iterate through the filtered process list and get more detailed statistics
  // about the filtered processes.
  for (proc_index = 0;
       proc_index < filtered_process_list->size;
       proc_index++) {
    int curr_pid =
        filtered_process_list->processes_e[proc_index].process_id;

    // Find the index of this process in prev_process_list. If it already
    // exists, set prev_process_list_idx to the index. Otherwise, it remains
    // invalid (-1).
    int i = 0;
    int prev_process_list_idx = -1;
    for (i = 0; i < prev_process_list->size; i++) {
      if (curr_pid == prev_process_list->processes_e[i].process_id) {
        prev_process_list_idx = i;
        break;
      }
    }

    // Find the index of this process in process_list. If it already
    // exists, set process_list_idx to the index. Otherwise, it remains invalid
    // (-1).
    int process_list_idx = -1;
    for (i = 0; i < process_list->size; i++) {
      if (curr_pid == process_list->processes_e[i].process_id) {
        process_list_idx = i;
        break;
      }
    }

    sprintf(pid_status_location, "/proc/%d/status", curr_pid);
    fp = fopen(pid_status_location, "r");
    if (fp == NULL) {
      // This means the process has gone shortly after we list the directory
      continue;
    }

    // Read /proc/*/status for information about:
    // - context switches
    // file format: http://man7.org/linux/man-pages/man5/proc.5.html
    // ...
    // voluntary_ctxt_switches:        150
    // nonvoluntary_ctxt_switches:     545
    unsigned long long voluntary_ctxt_switches, nonvoluntary_ctxt_switches;
    char line_buffer[4][256];
    char* prev_line = line_buffer[0];
    char* curr_line = line_buffer[1];
    char* next_line = line_buffer[2];
    while (fgets(next_line, 256, fp) != NULL) {
      // Rotate the pointers
      char* tmp_ptr = prev_line;
      prev_line = curr_line;
      curr_line = next_line;
      next_line = tmp_ptr;
    }
    fclose(fp);

    // We should expect prev_line, curr_line contain the last 2 lines
    int j = 0;
    while (!isspace(prev_line[j])) j++;
    voluntary_ctxt_switches = atoll(&prev_line[j]);
    j = 0;
    while (!isspace(curr_line[j])) j++;
    nonvoluntary_ctxt_switches = atoll(&curr_line[j]);

    sprintf(pid_io_location, "/proc/%d/io", curr_pid);
    fp = fopen(pid_io_location, "r");
    if (fp == NULL) {
      // This means the process has gone shortly after we list the directory
      continue;
    }

    // Read /proc/*/io for information about
    // - I/O
    // file format: http://man7.org/linux/man-pages/man5/proc.5.html
    // ...
    // read_bytes: 0
    // write_bytes: 323932160
    // cancelled_write_bytes: 0
    unsigned long long read_bytes, write_bytes;
    char* prev_prev_line = line_buffer[3];
    while (fgets(next_line, 256, fp) != NULL) {
      // Rotate the pointers
      char* tmp_ptr = prev_prev_line;
      prev_prev_line = prev_line;
      prev_line = curr_line;
      curr_line = next_line;
      next_line = tmp_ptr;
    }
    fclose(fp);

    // We should expect prev_prev_line, prev_line, curr_line contain the last
    // 3 lines
    j = 0;
    while (!isspace(prev_prev_line[j])) j++;
    read_bytes = atoll(&prev_prev_line[j]);
    j = 0;
    while (!isspace(prev_line[j])) j++;
    write_bytes = atoll(&prev_line[j]);

    // Context switches
    process_list->processes_i[process_list_idx].voluntary_ctxt_switches =
        voluntary_ctxt_switches;
    process_list->processes_i[process_list_idx]
        .nonvoluntary_ctxt_switches = nonvoluntary_ctxt_switches;
    // I/O
    process_list->processes_i[process_list_idx].read_bytes = read_bytes;
    process_list->processes_i[process_list_idx].write_bytes = write_bytes;

    // Get the CPU affinity information of all child processes/threads
    process_list->processes_e[process_list_idx].cpu_affinity = 0ULL;
    char child_dir_location[64];
    // Reset the threads count
    process_list->processes_i[process_list_idx].child_thread_ids_size = 0;
    // The chile processes/threads are located in /proc/*/task/
    sprintf(child_dir_location, "/proc/%d/task/", curr_pid);
    DIR* child_dir_ptr = opendir(child_dir_location);
    struct dirent* curr_child_dir_ptr;

    while ((curr_child_dir_ptr = readdir(child_dir_ptr)) != NULL) {
      if (curr_child_dir_ptr->d_name[0] >= '0' &&
          curr_child_dir_ptr->d_name[0] <= '9') {
        // Add the thread ID to the list
        if (process_list->processes_i[process_list->size]
                .child_thread_ids_size >= MAX_NUM_THREADS) {
          logging(LOG_CODE_FATAL,
                  "Process %d has too many threads (max is %d).\n",
                  curr_pid, MAX_NUM_THREADS);
        }
        process_list->processes_i[process_list->size]
            .child_thread_ids[process_list->processes_i[process_list->size]
                              .child_thread_ids_size] =
            atoi(curr_child_dir_ptr->d_name);
        process_list->processes_i[process_list->size].child_thread_ids_size++;
        // Check the affinity information
        char child_stat_location[64];
        sprintf(child_stat_location, "/proc/%d/task/%s/stat",
                curr_pid, curr_child_dir_ptr->d_name);
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
        process_list->processes_e[process_list->size].cpu_affinity |=
            (1ULL << processor);
      }
    }
    // Close the directory
    (void)closedir(child_dir_ptr);

    // The PID does not exist in the original list
    if (prev_process_list_idx == -1) {
      // Context switch rate
      process_list->processes_e[process_list_idx]
          .v_ctxt_switch_rate =
              (float)process_list->processes_i[process_list_idx]
                     .voluntary_ctxt_switches /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
      process_list->processes_e[process_list_idx].nv_ctxt_switch_rate =
          (float)process_list->processes_i[process_list_idx]
                     .nonvoluntary_ctxt_switches /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
      // I/O
      process_list->processes_e[process_list_idx].io_read_rate =
          (float)process_list->processes_i[process_list_idx].read_bytes /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
      process_list->processes_e[process_list_idx].io_write_rate =
          (float)process_list->processes_i[process_list_idx].write_bytes /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
    // The PID is in the original list
    } else {
      // Context switch rate
      process_list->processes_e[process_list_idx].v_ctxt_switch_rate =
          (float)(process_list->processes_i[process_list_idx]
                      .voluntary_ctxt_switches -
                  prev_process_list->processes_i[prev_process_list_idx]
                      .voluntary_ctxt_switches) /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
      process_list->processes_e[process_list_idx].nv_ctxt_switch_rate =
          (float)(process_list->processes_i[process_list_idx]
                      .nonvoluntary_ctxt_switches -
                  prev_process_list->processes_i[prev_process_list_idx]
                      .nonvoluntary_ctxt_switches) /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
      // I/O
      process_list->processes_e[process_list_idx].io_read_rate =
          (float)(process_list->processes_i[process_list_idx].read_bytes -
                  prev_process_list->processes_i[prev_process_list_idx].read_bytes) /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
      process_list->processes_e[process_list_idx].io_write_rate =
          (float)(process_list->processes_i[process_list_idx]
                      .write_bytes -
                  prev_process_list->processes_i[prev_process_list_idx].write_bytes) /
          (process_list->cpu_total_time -
           prev_process_list->cpu_total_time);
    }

    // Fill the numbers to the filtered list
    filtered_process_list->processes_e[proc_index].v_ctxt_switch_rate =
        process_list->processes_e[process_list_idx].v_ctxt_switch_rate;
    filtered_process_list->processes_e[proc_index].nv_ctxt_switch_rate =
        process_list->processes_e[process_list_idx].nv_ctxt_switch_rate;
    filtered_process_list->processes_e[proc_index].io_read_rate =
        process_list->processes_e[process_list_idx].io_read_rate;
    filtered_process_list->processes_e[proc_index].io_write_rate =
        process_list->processes_e[process_list_idx].io_write_rate;
  }
}

void filter_process_info(process_list_t* process_list,
                         process_list_t* filtered_process_list,
                         int num_of_processes) {
  // Create a temporary index array to avoid modification on the original array
  int temp_index_list[MAX_NUM_PROCESSES];
  int i, j;
  for (i = 0; i < process_list->size; i++) {
    temp_index_list[i] = i;
  }

  // Find the k largest elements based on CPU utilization
  for (i = 0; i < num_of_processes; i++) {
    int max_index = i;
    float max_value =
        process_list->processes_e[temp_index_list[i]].cpu_utilization;
    for (j = i + 1; j < process_list->size; j++) {
      if (process_list->processes_e[temp_index_list[j]].cpu_utilization >
          max_value) {
        max_index = j;
        max_value =
            process_list->processes_e[temp_index_list[j]].cpu_utilization;
      }
    }
    // Swap
    int temp = temp_index_list[i];
    temp_index_list[i] = temp_index_list[max_index];
    temp_index_list[max_index] = temp;
  }

  // Copy the k largest elemented to the filtered list
  filtered_process_list->cpu_total_time =
      process_list->cpu_total_time;
  filtered_process_list->size = num_of_processes;
  for (i = 0; i < num_of_processes; i++) {
    filtered_process_list->processes_e[i] =
        process_list->processes_e[temp_index_list[i]];
    filtered_process_list->processes_i[i] =
        process_list->processes_i[temp_index_list[i]];
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
    printf("  PID: %u, minflt: %lu, cminflt: %lu, majflt: %lu, cmajflt: %lu, "
           "tflt: %lu, utime: %lu, stime: %lu, cutime: %lu, cstime: %lu, "
           "ttime: %lu, voluntary_ctxt_switches: %llu, "
           "nonvoluntary_ctxt_switches: %llu, read_bytes: %llu, "
           "write_bytes: %llu, page_fault_rate: %f, "
           "cpu_utilization: %f, v_ctxt_switch_rate: %f, "
           "nv_ctxt_switch_rate: %f, io_read_rate: %f, io_write_rate: %f, "
           "virtial_mem_utilization: %f, real_mem_utilization: %f\n",
           process_list->processes_e[i].process_id,
           process_list->processes_i[i].minflt,
           process_list->processes_i[i].cminflt,
           process_list->processes_i[i].majflt,
           process_list->processes_i[i].cmajflt,
           process_list->processes_i[i].tflt,
           process_list->processes_i[i].utime,
           process_list->processes_i[i].stime,
           process_list->processes_i[i].cutime,
           process_list->processes_i[i].cstime,
           process_list->processes_i[i].ttime,
           process_list->processes_i[i].voluntary_ctxt_switches,
           process_list->processes_i[i].nonvoluntary_ctxt_switches,
           process_list->processes_i[i].read_bytes,
           process_list->processes_i[i].write_bytes,
           process_list->processes_e[i].page_fault_rate,
           process_list->processes_e[i].cpu_utilization,
           process_list->processes_e[i].v_ctxt_switch_rate,
           process_list->processes_e[i].nv_ctxt_switch_rate,
           process_list->processes_e[i].io_read_rate,
           process_list->processes_e[i].io_write_rate,
           process_list->processes_e[i].virtual_mem_utilization,
           process_list->processes_e[i].real_mem_utilization);
  }
}
