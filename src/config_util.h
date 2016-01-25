/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __CONFIG_UTIL_H__
#define __CONFIG_UTIL_H__

#include "app_sample.h"
#include "pmu_sample.h"

typedef struct {
  const char* events[MAX_EVENTS];
  char events_buffer[MAX_EVENTS][PMU_EVENTS_NAME_LENGTH];
  char* config_file;
  char applications[MAX_NUM_APPLICATIONS][MAX_APP_NAME_LENGTH];
  char hostnames[MAX_NUM_APPLICATIONS][MAX_HOSTNAME_LENGTH];
  unsigned int ports[MAX_NUM_APPLICATIONS];
  int num_of_applications;
  int num_of_processes;
  int interval_us;
  char* output_file;
} options_t;

void parse_config(char* config, options_t* options,
                  hardware_info_t* hardware_info);

#endif
