/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "proc_sample.h"
//using namespace std;

struct process_list *get_process_id() {
  dp = opendir ("/proc/");
  struct process_list *curr;
  struct process_list *head;
  head = (struct process_list *) malloc(sizeof (*head));
  //processList * curr, * head;
  head = NULL;
     if (dp != NULL) {
           while ((ep = readdir (dp))) {   
     //        both[sizeof(both)-1] = '\0';
     //      
               if(ep->d_name[0]>=48 && ep->d_name[0]<=57) {

      curr = (struct process_list *)malloc(sizeof *curr);
      curr->processId = atoi(ep->d_name);
      curr->link  = head;
      head = curr;
     //              strcpy(both,first);
     //              strcat(both, ep->d_name);
     //              strcat(both, "/stat");
     //              //printf(" command executed is %s \n",both);
     //            fp = fopen(both, "r");
     //              
     //              if(fp == NULL){
     //                  printf("\nFile doesnt Exist");
     //                  exit(0);
     //              } else {
     //                  while(!feof(fp)){
     //                      for(i=0; i<5; i++){
     //                              ch = fgetc(fp);
     //                                  if(ch != search[i])
     //                                  break;
     //                  }
     //                      if(i==5){
        //puts (ep->d_name);
     //                          list_vms[++count_list_vm][0] = atoi(ep->d_name);
     //                      }
     //                  }
 
     //              }
     //              fclose(fp);
               }
           }
                   
            (void) closedir (dp);
         }
         else
               perror ("Couldn't open the directory");
 	//curr = head;

 	//  while(curr) {
 	//     printf("%d --\n", curr->processId);
 	//     curr = curr->link ;
 	//  }
	printf("ram srivatsa\n");
	return head;

}

