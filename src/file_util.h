/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#ifndef __FILE_UTIL__
#define __FILE_UTIL__

#include <stdbool.h>

void read_file(char* filename, char* read_buffer, unsigned int buffer_size);

void write_file(char* filename, char* write_buffer, unsigned int size,
                bool append);

#endif
