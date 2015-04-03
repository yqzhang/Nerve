/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "app_sample.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

application_list_t application_list;

int init_app_sample(char** hostnames, unsigned int* ports,
                    unsigned int num_applications) {
  application_list.size = 0;

  int i;
  for (i = 0; i < num_applications; i++) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      fprintf(stderr, "Error opening socket.\n");
      exit(1);
    }
    struct hostent* server = gethostbyname(hostnames[i]);
    if (server == NULL) {
      fprintf(stderr, "Error finding host %s.\n", hostnames[i]);
      exit(1);
    }
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&server_addr.sin_addr.s_addr,
          server->h_length);
    server_addr.sin_port = htons(ports[i]);
    if (connect(sockfd, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr_in)) < 0) {
      fprintf(stderr, "Error connecting to host %s at port %d.\n",
              hostnames[i], ports[i]);
      exit(1);
    }

    // Record the values in application_list
    strcpy(application_list.applications[application_list.size].hostname,
           hostnames[i]);
    application_list.applications[application_list.size].port = ports[i];
    application_list.applications[application_list.size].sockfd = sockfd;
    application_list.size++;

    // Reset the statistics
  }

  return 0;
}

void get_app_sample() {
  int i = 0;
  for (i = 0; i < application_list.size; i++) {

  }
}

void clean_app_sample() {
  int i;
  for (i = 0; i < application_list.size; i++) {
    close(application_list.applications[i].sockfd);
  }
}
