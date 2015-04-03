/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <err.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "app_sample.h"
#include "log_util.h"
#include "pmu_sample.h"
#include "proc_sample.h"

typedef struct {
  char* events[MAX_GROUPS];
  int num_groups;
  int print;
  int pin;
  char* hostnames[MAX_NUM_APPLICATIONS];
  unsigned int ports[MAX_NUM_APPLICATIONS];
  int num_applications;
} options_t;

options_t options;
// TODO: maybe use this for in sig_handler
static volatile int quit;

static void sig_handler(int n) {
  // TODO: handle interrupts
  exit(0);
}

static void usage(void) {
  printf(
      "usage: task [-h] [-g] [-p] [-P] [-e event1,event2,...] cmd\n"
      "-h\t\tget help\n"
      "-p\t\tprint counts every second\n"
      "-P\t\tpin events\n"
      "-e ev,ev\tgroup of events to measure (multiple -e switches are "
      "allowed)\n");
}

int main(int argc, char** argv) {
  int c;

  setlocale(LC_ALL, "");

  while ((c = getopt(argc, argv, "+he:pP:")) != -1) {
    switch (c) {
      case 'e':
        if (options.num_groups < MAX_GROUPS) {
          options.events[options.num_groups++] = optarg;
        } else {
          logging(LOG_CODE_FATAL, "you cannot specify more than %d groups.\n",
                  MAX_GROUPS);
        }
        break;
      case 'p':
        options.print = 1;
        break;
      case 'P':
        options.pin = 1;
        break;
      case 'h':
        usage();
        exit(0);
      default:
        logging(LOG_CODE_FATAL, "unknown error");
    }
  }

  // TODO: Read this from configuration file
  if (options.num_groups == 0) {
    options.events[0] = "cycles,instructions";
    options.num_groups = 1;
  }
  options.hostnames[0] = "localhost";
  options.ports[0] = 1234;
  options.num_applications = 1;

  signal(SIGINT, sig_handler);

  // Create an array so that we can keep reusing them
  process_list_t process_info_array[3];
  process_list_t* process_info_list = &process_info_array[0];
  process_list_t* prev_process_info_list = &process_info_array[1];
  process_list_t* filtered_process_info_list = &process_info_array[2];

  // Initialize the application sampling
  init_app_sample(options.hostnames, options.ports, options.num_applications);

  while (true) {
    // Sample all the running processes, and calculate their utilization
    // information in the last sample interval
    get_process_info(process_info_list, prev_process_info_list);

    // Filter the list of processes by a list of thresholds
    filter_process_info(process_info_list, filtered_process_info_list, 0.15);

    // Profile all the PMU events of all the processes in the list,
    // and sleep for the same time as sample interval
    get_pmu_sample(filtered_process_info_list, (const char**)options.events,
                   1e6);

    // Get performance statistics from the applications
    get_app_sample();

    swap_process_list(&process_info_list, &prev_process_info_list);
  }

  // Clean up application sampling
  clean_app_sample();

  return 0;
}
