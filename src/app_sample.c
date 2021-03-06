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

#include "log_util.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

application_list_t application_list;

void establish_connection(int app_index) {
  // For simplicity
  char* hostname = application_list.applications[app_index].hostname;
  int port = application_list.applications[app_index].port;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    logging(LOG_CODE_FATAL, "Error opening socket.\n");
  }
  struct hostent* server = gethostbyname(hostname);
  if (server == NULL) {
    logging(LOG_CODE_FATAL, "Error finding host %s.\n", hostname);
  }
  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&server_addr.sin_addr.s_addr,
        server->h_length);
  server_addr.sin_port = htons(port);
  if (connect(sockfd, (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_in)) < 0) {
    logging(LOG_CODE_WARNING, "Error connecting to host %s at port %d.\n",
            hostname, port);
    // We need to try again later
    application_list.applications[app_index].connected = false;
  } else {
    // This connection is good to go
    application_list.applications[app_index].sockfd = sockfd;
    application_list.applications[app_index].connected = true;
    // Reset the statistics
    snoop_reply_t reply;
    send_request(sockfd, SNOOP_CMD_RESET, &reply);
    if (reply.snoop_reply_code == SNOOP_REPLY_ERROR) {
      logging(LOG_CODE_FATAL, "Error resetting statistics on %s:%d.\n",
              hostname, port);
    }
  }
}

int init_app_sample(char hostnames[MAX_NUM_APPLICATIONS][MAX_HOSTNAME_LENGTH],
                    unsigned int ports[MAX_NUM_APPLICATIONS],
                    unsigned int num_applications) {
  application_list.size = 0;

  int i;
  for (i = 0; i < num_applications; i++) {
    // Record the values in application_list
    strcpy(application_list.applications[application_list.size].hostname,
           hostnames[i]);
    application_list.applications[application_list.size].port = ports[i];
    establish_connection(application_list.size);
    application_list.size++;
  }

  return 0;
}

void get_app_sample() {
  int i = 0;
  snoop_reply_t snoop_reply;
  for (i = 0; i < application_list.size; i++) {
    // Check if we have already established the connection
    if (application_list.applications[i].connected == true) {
      // Snoop the performance statistics
      send_request(application_list.applications[i].sockfd, SNOOP_CMD_PERF,
                   &snoop_reply);

      // TODO: Do something with the reply here

      // Reset the statistics
      send_request(application_list.applications[i].sockfd, SNOOP_CMD_RESET,
                   &snoop_reply);
      if (snoop_reply.snoop_reply_code == SNOOP_REPLY_ERROR) {
        logging(LOG_CODE_FATAL, "Error resetting statistics on %s:%d.\n",
                application_list.applications[i].hostname,
                application_list.applications[i].port);
      }
    // Otherwise we will need to build the connection
    } else {
      establish_connection(i);
    }
  }
}

void clean_app_sample() {
  int i;
  for (i = 0; i < application_list.size; i++) {
    if (application_list.applications[i].connected == true) {
      close(application_list.applications[i].sockfd);
    }
  }
}

void send_request(int sockfd, short snoop_command, snoop_reply_t* reply) {
  snoop_request_t request;
  request.snoop_command = snoop_command;
  int n = write(sockfd, (char *)&request, sizeof(snoop_request_t));
  if (n < 0) {
    logging(LOG_CODE_FATAL, "Error writing to socket.\n");
  }
  n = read(sockfd, (char *)&reply, sizeof(snoop_reply_t));
  if (n < 0) {
    logging(LOG_CODE_FATAL, "Error reading from socket.\n");
  }
}
