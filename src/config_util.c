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
#include <string.h>

#include "config_util.h"
#include "log_util.h"

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
