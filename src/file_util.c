/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "file_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void read_file(char* filename, char* read_buffer, unsigned int buffer_size) {
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file %s.\n", filename);
    exit(1);
  }

  fread(read_buffer, buffer_size, 1, fp);
  fclose(fp);
}

void write_file(char* filename, char* write_buffer, unsigned int size,
                bool append) {
  if (size == 0) {
    size = strlen(write_buffer);
  }

  FILE* fp;
  if (append == true) {
    fp = fopen(filename, "a");
  } else {
    fp = fopen(filename, "w");
  }

  if (fp == NULL) {
    fprintf(stderr, "Error openning file %s.\n", filename);
    exit(1);
  }

  fwrite(write_buffer, 1, sizeof(char), fp);
  fclose(fp);
}
