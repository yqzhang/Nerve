/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <dirent.h>
#include <sys/types.h>
#ifndef __PROC_SAMPLE_H__
#define __PROC_SAMPLE_H__

#endif

struct process_list {
   int processId;
   struct process_list *link;
};

//typedef struct process_list processList;

struct process_list *get_process_id();
DIR *dp;
struct dirent *ep;


