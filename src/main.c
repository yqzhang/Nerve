/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <jansson.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_sample.h"
#include "file_util.h"
#include "log_util.h"
#include "pmu_sample.h"
#include "proc_sample.h"

// Buffer size allocated for the JSON fomatted config file
#define JSON_BUFFER_SIZE 4 * 1024

// Max number of PMU events that can be used in each group
#define PMU_EVENTS_PER_GROUP 5

// Max length of each event list
#define PMU_EVENTS_NAME_LENGTH 256

typedef struct {
  char events[MAX_GROUPS][PMU_EVENTS_NAME_LENGTH];
  int num_groups;
  char* config_file;
  const char* applications[MAX_NUM_APPLICATIONS];
  const char* hostnames[MAX_NUM_APPLICATIONS];
  unsigned int ports[MAX_NUM_APPLICATIONS];
  int num_applications;
} options_t;

// TODO: maybe use this for in sig_handler
static volatile int quit;

static void sig_handler(int n) {
  // TODO: handle interrupts
  exit(0);
}

static void usage(void) {
  printf(
      "usage: neuron [-h] [-c config.json]\n"
      "-h\t\tget help\n"
      "-c config.json\tconfiguration file in JSON format (required)\n");
}

void parse_config(char* config, options_t* options) {
  json_t* json_root;
  json_error_t json_error;
  json_root = json_loads(config, 0, &json_error);
  if (json_root == NULL) {
    logging(LOG_CODE_FATAL,
            "Error parsing JSON config file at line %d, column %d: %s\n",
            json_error.line, json_error.column, json_error.text);
  }

  // Application information
  json_t* app_dict = json_object_get(json_root, "application");
  const char* app_key;
  json_t* app_value;
  options->num_applications = 0;
  json_object_foreach (app_dict, app_key, app_value) {
    if (options->num_applications >= MAX_NUM_APPLICATIONS) {
      logging(LOG_CODE_FATAL, "Too many applications to monitor (max is %d)\n",
              MAX_NUM_APPLICATIONS);
    }

    // Parse hostname
    json_t* json_hostname = json_object_get(app_value, "hostname");
    // Check if hostname is provided
    if (json_hostname == NULL) {
      logging(LOG_CODE_FATAL,
              "Application %s does not contain a hostname (required).\n",
              app_key);
    // Check if the provided hostname is a string
    } else if (!json_is_string(json_hostname)) {
      logging(LOG_CODE_FATAL,
              "The hostname of application %s is not a string.\n", app_key);
    }
    // Parse port
    json_t* json_port = json_object_get(app_value, "port");
    // Check if port is provided
    if (json_hostname == NULL) {
      logging(LOG_CODE_FATAL,
              "Application %s does not contain a port (required).\n", app_key);
    // Check if the provided port is an integer
    } else if (!json_is_integer(json_port)) {
      logging(LOG_CODE_FATAL,
              "The hostname of application %s is not an integer.\n", app_key);
    }

    // Record the parsed information into options
    options->applications[options->num_applications] = app_key;
    options->hostnames[options->num_applications] =
        json_string_value(json_hostname);
    options->ports[options->num_applications] =
        json_integer_value(json_port);
    logging(LOG_CODE_INFO, "Start monitoring application: %s at %s:%d.\n",
            options->applications[options->num_applications],
            options->hostnames[options->num_applications],
            options->ports[options->num_applications]);

    // Increment the application counter
    options->num_applications++;
  }

  // PMU counters
  options->num_groups = 1;
  int num_events_in_group = 0;
  json_t* pmu_list = json_object_get(json_root, "pmu");
  size_t pmu_index;
  json_t* pmu_value;

  json_array_foreach (pmu_list, pmu_index, pmu_value) {
    if (!json_is_string(pmu_value)) {
      logging(LOG_CODE_FATAL,
              "The %zuth PMU event is not an integer.\n", pmu_index + 1);
    }
    // Start the new group
    if (num_events_in_group == PMU_EVENTS_PER_GROUP) {
      options->num_groups++;
      num_events_in_group = 0;
    }
    const char* pmu_name = json_string_value(pmu_value);
    // First event in the group
    if (num_events_in_group == 0) {
      strcpy(options->events[options->num_groups - 1], pmu_name);
    // Later on events
    } else {
      strcat(options->events[options->num_groups - 1], pmu_name);
      strcat(options->events[options->num_groups - 1], ",");
    }
    num_events_in_group++;

    logging(LOG_CODE_INFO, "PMU event %s registered in group %u.\n",
            pmu_name, options->num_groups - 1);
  }

  // Clean up
  json_decref(json_root);
}

int main(int argc, char** argv) {
  int c;
  options_t options;

  setlocale(LC_ALL, "");

  while ((c = getopt(argc, argv, "+hc:")) != -1) {
    switch (c) {
      case 'h':
        usage();
        exit(0);
      case 'c':
        options.config_file = optarg;
        break;
      default:
        logging(LOG_CODE_FATAL, "unknown error");
    }
  }

  // Load the JSON formatted config file
  if (options.config_file == NULL) {
    usage();
    logging(LOG_CODE_FATAL, "Config JSON file is required.\n");
  }
  char json_buffer[JSON_BUFFER_SIZE];
  read_file(options.config_file, json_buffer, JSON_BUFFER_SIZE);

  // Parse the JSON config file
  parse_config(json_buffer, &options);

  signal(SIGINT, sig_handler);

  // Create an array so that we can keep reusing them
  process_list_t process_info_array[3];
  process_list_t* process_info_list = &process_info_array[0];
  process_list_t* prev_process_info_list = &process_info_array[1];
  process_list_t* filtered_process_info_list = &process_info_array[2];

  // FIXME: Initialize the application sampling
  // init_app_sample(options.hostnames, options.ports, options.num_applications);

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

    // FIXME: Get performance statistics from the applications
    // get_app_sample();

    swap_process_list(&process_info_list, &prev_process_info_list);
  }

  // FIXME: Clean up application sampling
  // clean_app_sample();

  return 0;
}
