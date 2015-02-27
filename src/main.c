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

#include "pmu_sample.h"
#include "proc_sample.h"


options_t options;
// TODO: maybe use this for in sig_handler
static volatile int quit;

static void sig_handler(int n) {
  // TODO: handle interrupts
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

int main(int argc, char **argv) {
  int c;

  setlocale(LC_ALL, "");

  while ((c = getopt(argc, argv, "+he:pP:")) != -1) {
    switch (c) {
      case 'e':
        if (options.num_groups < MAX_GROUPS) {
          options.events[options.num_groups++] = optarg;
        } else {
          errx(1, "you cannot specify more than %d groups.\n", MAX_GROUPS);
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
        errx(1, "unknown error");
    }
  }
  if (options.num_groups == 0) {
    options.events[0] = "cycles,instructions";
    options.num_groups = 1;
  }

  signal(SIGINT, sig_handler);

  process_info_node_t* process_info_list;
  process_info_node_t* curr_ptr;
  int size=0;
  if (true) {
    // TODO: take a snapshot of the running processes
    // @ram
    process_info_list = get_process_info();
    curr_ptr = process_info_list;
    while (curr_ptr!=NULL) {
      size++;
      curr_ptr=curr_ptr->next;
    }
    if (size > 2) size = 2;
    // TODO: take the lists of PIDs and events
    // @zhiyi @xianghan
    get_event_info(process_info_list, size, &options);
  }

  return 0;
}
