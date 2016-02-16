/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "app_sample.h"
#include "config_util.h"
#include "file_util.h"
#include "log_util.h"
#include "pmu_sample.h"
#include "proc_sample.h"

// Buffer size allocated for the JSON fomatted config file
#define JSON_BUFFER_SIZE 4 * 1024

static void sig_handler(int n) {
  clean_app_sample();
  clean_pmu_sample();

  exit(0);
}

static void usage(void) {
  printf(
      "usage: nerve [-h] [-i 1000] [-c config.json] [-o output.bin]\n"
      "-h\t\tget help\n"
      "-i 1000\tsample interval in milliseconds (required)\n"
      "-c config.json\tconfiguration file in JSON format (required)\n"
      "-o output.bin\toutput file in binary format (required)\n");
}

int main(int argc, char** argv) {
  int c;
  options_t options;
  options.interval_us = -1;
  options.output_file = NULL;

  setlocale(LC_ALL, "");

  while ((c = getopt(argc, argv, "+hi:c:o:")) != -1) {
    switch (c) {
      case 'h':
        usage();
        exit(0);
      case 'i':
        options.interval_us = 1000 * atoi(optarg);
        break;
      case 'c':
        options.config_file = optarg;
        break;
      case 'o':
        options.output_file = optarg;
        break;
      default:
        logging(LOG_CODE_FATAL, "Unknown argument");
    }
  }

  // Check if we have the sample interval
  if (options.interval_us == -1) {
    usage();
    logging(LOG_CODE_FATAL, "Sample interval is required.\n");
  }

  // Check if we have the output file
  if (options.output_file == NULL) {
    usage();
    logging(LOG_CODE_FATAL, "Output file is required.\n");
  }

  // Load the JSON formatted config file
  if (options.config_file == NULL) {
    usage();
    logging(LOG_CODE_FATAL, "Config JSON file is required.\n");
  }
  char json_buffer[JSON_BUFFER_SIZE];
  read_file(options.config_file, json_buffer, JSON_BUFFER_SIZE);

  // Create an array so that we can keep reusing them
  process_list_t process_info_array[3];
  process_list_t* process_info_list = &process_info_array[0];
  process_list_t* prev_process_info_list = &process_info_array[1];
  process_list_t* filtered_process_info_list = &process_info_array[2];

  // Create a struct for the hardware-related information
  hardware_info_t hardware_info;

  // Parse the JSON config file
  parse_config(json_buffer, &options, &hardware_info);

  signal(SIGINT, sig_handler);

  // Initialize the application sampling
  init_app_sample(options.hostnames, options.ports,
                  options.num_of_applications);
  init_pmu_sample(&hardware_info);

  int nerve_pid = (int) getpid();

  /**
   * This is the main loop where all the monitoring happens. In each interation:
   *  1) find out the top cycle-consuming running processes
   *  2) collect OS-level statistics about the processes
   *  3) collect hardware PMUs statistics
   *  4) collect statistics reported by the applications
   *  5) dump all the statistics to a file
   */
  while (true) {
    // Sample all the running processes, and calculate their utilization
    // information in the last sample interval
    get_process_info(process_info_list, prev_process_info_list, nerve_pid);

    // Filter the list of processes by a list of thresholds
    filter_process_info(process_info_list, filtered_process_info_list,
                        options.num_of_processes);

    // Get more detailed statistics about running processes
    get_process_stats(filtered_process_info_list,
                      process_info_list,
                      prev_process_info_list);

    // Profile all the PMU events of all the processes in the list,
    // and sleep for the same time as sample interval
    get_pmu_sample(filtered_process_info_list, options.events,
                   options.interval_us, &hardware_info);

    // Get performance statistics from the applications
    // get_app_sample();

    // Record all the information
    write_all(options.output_file, true, hardware_info.num_of_cores,
              options.num_of_processes, hardware_info.num_of_events,
              hardware_info.irq_info, hardware_info.network_info,
              hardware_info.frequency_info,
              filtered_process_info_list->processes_e,
              hardware_info.pmu_info);

    swap_process_list(&process_info_list, &prev_process_info_list);
  }

  // Clean up application sampling
  clean_app_sample();
  clean_pmu_sample();

  return 0;
}
