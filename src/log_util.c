/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "log_util.h"

#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int logging(short code, const char* format, ...) {
  time_t timer;
  char time_buffer[26];
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);
  strftime(time_buffer, 26, "%Y/%m/%d %H:%M:%S", tm_info);

  va_list args;
  va_start(args, format);
  switch(code) {
    case(LOG_CODE_INFO):
      fprintf(stdout, "[INFO] [%s] ", time_buffer);
      return vfprintf(stdout, format, args);
    case(LOG_CODE_WARNING):
      fprintf(stderr, "[WARNING] [%s] ", time_buffer);
      return vfprintf(stderr, format, args);
    case(LOG_CODE_FATAL):
      fprintf(stderr, "[FATAL] [%s] ", time_buffer);
      vfprintf(stderr, format, args);
      exit(1);
    default:
      return 0;
  }
  va_end(args);
}
