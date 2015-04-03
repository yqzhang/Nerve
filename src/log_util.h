/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __LOG_UTIL__
#define __LOG_UTIL__

typedef enum {
  LOG_CODE_INFO = 0x00,
  LOG_CODE_WARNING = 0x01,
  LOG_CODE_FATAL = 0x02,
} log_code_t;

int logging(short code, const char* format, ...);

#endif
