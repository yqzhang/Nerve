/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __APP_SAMPLE_H__
#define __APP_SAMPLE_H__

#include <sys/types.h>

#define MAX_HOSTNAME_LENGTH 32
#define MAX_NUM_APPLICATIONS 8

typedef struct application {
  char hostname[MAX_HOSTNAME_LENGTH];
  unsigned int port;
} application_t;

typedef struct application_list {
  application_t applications[MAX_NUM_APPLICATIONS];
  size_t size;
} application_list_t;

// All possible commands that can be sent as snoop request
typedef enum {
  SNOOP_CMD_RESET = 0x00,
  SNOOP_CMD_PERF = 0x01,
} snoop_command_t;

typedef struct snoop_request {
  short snoop_command;
} snoop_request_t;

// Possible return code of a snoop request
typedef enum {
  SNOOP_REPLY_SUCCESS = 0x00,
  SNOOP_REPLY_ERROR = 0x01,
} snoop_reply_code_t;

typedef struct snoop_reply {
  // return code
  short snoop_reply_code;
  // total number of requests that have been sent
  unsigned long num_of_reuqests;
  // tail latency of the sent requests microseconds
  double tail_latency;
} snoop_reply_t;

#endif
