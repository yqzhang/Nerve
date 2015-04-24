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
#define PMU_EVENTS_NAME_LENGTH 64

// PMU for NUMA local accesses
#define PMU_NUMA_LMA "OFFCORE_RESPONSE_1:DMND_DATA_RD:LLC_MISS_LOCAL:SNP_MISS:SNP_NO_FWD"

// PMU for NUMA remote accesses
#define PMU_NUMA_RMA "OFFCORE_RESPONSE_0:DMND_DATA_RD:LLC_MISS_REMOTE:SNP_MISS:SNP_NO_FWD"

typedef struct {
  const char* events[MAX_EVENTS];
  char events_buffer[MAX_EVENTS][PMU_EVENTS_NAME_LENGTH];
  char* config_file;
  char applications[MAX_NUM_APPLICATIONS][MAX_APP_NAME_LENGTH];
  char hostnames[MAX_NUM_APPLICATIONS][MAX_HOSTNAME_LENGTH];
  unsigned int ports[MAX_NUM_APPLICATIONS];
  int num_of_applications;
  int num_of_processes;
} options_t;

static void sig_handler(int n) {
  clean_app_sample();
  clean_pmu_sample();

  exit(0);
}

static void usage(void) {
  printf(
      "usage: neuron [-h] [-c config.json]\n"
      "-h\t\tget help\n"
      "-c config.json\tconfiguration file in JSON format (required)\n");
}

void parse_config(char* config, options_t* options,
                  hardware_info_t* hardware_info) {
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
  options->num_of_applications = 0;
  json_object_foreach (app_dict, app_key, app_value) {
    if (options->num_of_applications >= MAX_NUM_APPLICATIONS) {
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
    strcpy(options->applications[options->num_of_applications], app_key);
    strcpy(options->hostnames[options->num_of_applications],
           json_string_value(json_hostname));
    options->ports[options->num_of_applications] =
        json_integer_value(json_port);
    logging(LOG_CODE_INFO, "Start monitoring application: %s at %s:%d.\n",
            options->applications[options->num_of_applications],
            options->hostnames[options->num_of_applications],
            options->ports[options->num_of_applications]);

    // Increment the application counter
    options->num_of_applications++;
  }

  // PMU counters
  int num_of_events = 0;
  json_t* pmu_list = json_object_get(json_root, "pmu");
  size_t pmu_index;
  json_t* pmu_value;

  memset(options->events, 0, sizeof(options->events));

  json_array_foreach (pmu_list, pmu_index, pmu_value) {
    if (!json_is_string(pmu_value)) {
      logging(LOG_CODE_FATAL,
              "The %zuth PMU event is not an integer.\n", pmu_index + 1);
    }
    options->events[num_of_events] =
        (const char*)&options->events_buffer[num_of_events];
    const char* pmu_name = json_string_value(pmu_value);
    strcpy(options->events_buffer[num_of_events], pmu_name);
    num_of_events++;

    logging(LOG_CODE_INFO, "PMU event %s registered.\n", pmu_name);
  }

  // Add PMU counters for NUMA events (local memory access)
  options->events[num_of_events] =
      (const char*)&options->events_buffer[num_of_events];
  strcpy(options->events_buffer[num_of_events], PMU_NUMA_LMA);
  num_of_events++;
  logging(LOG_CODE_INFO, "PMU event %s registered.\n", PMU_NUMA_LMA);

  // Add PMU counters for NUMA events (remote memory access)
  options->events[num_of_events] =
      (const char*)&options->events_buffer[num_of_events];
  strcpy(options->events_buffer[num_of_events], PMU_NUMA_RMA);
  num_of_events++;
  logging(LOG_CODE_INFO, "PMU event %s registered.\n", PMU_NUMA_RMA);

  hardware_info->num_of_events = num_of_events;

  // Number of processes to monitor that are utilizing the most resources
  json_t* num_of_processes = json_object_get(json_root, "num_of_processes");
  options->num_of_processes = json_integer_value(num_of_processes);
  logging(LOG_CODE_INFO, "Monitoring the top %d processes.\n",
          options->num_of_processes);

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
        logging(LOG_CODE_FATAL, "Unknown argument");
    }
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

  while (true) {
    // Sample all the running processes, and calculate their utilization
    // information in the last sample interval
    get_process_info(process_info_list, prev_process_info_list);

    // Filter the list of processes by a list of thresholds
    filter_process_info(process_info_list, filtered_process_info_list,
                        options.num_of_processes);

    // Profile all the PMU events of all the processes in the list,
    // and sleep for the same time as sample interval
    // TODO: Change the sample interval to an argument
    get_pmu_sample(filtered_process_info_list, options.events, 1e6,
                   &hardware_info);

    // Get performance statistics from the applications
    get_app_sample();

    // Record all the information
    // TODO: Change the file name to an argument
    write_all("temp.txt", true, hardware_info.num_of_cores,
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
