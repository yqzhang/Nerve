/*
 *  Copyright (c) 2015, University of Michigan.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "pmu_sample.h"
#include "proc_sample.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

// TODO: replace these options with parameters passed through the function call
//int todo_option_pid = 0;
int todo_option_print = 0;
//int todo_option_num_groups = 1;
//char *todo_option_events[MAX_GROUPS];
int todo_option_quit = 0;

/* No use here
int child(char **arg) {
  *
   * execute the requested command
   */
  /*
  execvp(arg[0], arg);
  errx(1, "cannot exec: %s\n", arg[0]);
}*/

void read_groups(perf_event_desc_t *fds, int num) {
  uint64_t *values = NULL;
  size_t new_sz, sz = 0;
  int i, evt;
  ssize_t ret;

  /*
   * 	{ u64		nr;
   * 	  { u64		time_enabled; } && PERF_FORMAT_ENABLED
   * 	  { u64		time_running; } && PERF_FORMAT_RUNNING
   * 	  { u64		value;
   * 	    { u64	id;           } && PERF_FORMAT_ID
   * 	  }		cntr[nr];
   * 	} && PERF_FORMAT_GROUP
   *
   * we do not use FORMAT_ID in this program
   */

  for (evt = 0; evt < num;) {
    int num_evts_to_read;

    num_evts_to_read = 1;
    new_sz = sizeof(uint64_t) * 3;

    if (new_sz > sz) {
      sz = new_sz;
      values = realloc(values, sz);
    }

    if (!values) {
      err(1, "cannot allocate memory for values\n");
    }

    ret = read(fds[evt].fd, values, new_sz);
    if (ret != (ssize_t)new_sz) {/* unsigned */
      if (ret == -1) {
        err(1, "cannot read values event %s", fds[evt].name);
      }

      /* likely pinned and could not be loaded */
      warnx("could not read event %d, tried to read %zu bytes, but got %zd",
            evt, new_sz, ret);
    }

    /*
     * propagate to save area
     */
    for (i = evt; i < (evt + num_evts_to_read); i++) {
      /*
       * scaling because we may be sharing the PMU and
       * thus may be multiplexed
       */
      fds[i].values[0] = values[0];
      fds[i].values[1] = values[1];
      fds[i].values[2] = values[2];
    }
    evt += num_evts_to_read;
  }
  if (values) {
    free(values);
  }
}

void print_counts(perf_event_desc_t *fds, int num) {
  double ratio;
  uint64_t val, delta;
  int i;

  read_groups(fds, num);

  for (i = 0; i < num; i++) {
    val = perf_scale(fds[i].values);
    delta = perf_scale_delta(fds[i].values, fds[i].prev_values);
    ratio = perf_scale_ratio(fds[i].values);

    /* separate groups */
    if (perf_is_group_leader(fds, i)) putchar('\n');

    if (todo_option_print) {
      printf("%'20" PRIu64 " %'20" PRIu64 " %s (%.2f%% scaling, ena=%'" PRIu64
             ", run=%'" PRIu64 ")\n",
             val, delta, fds[i].name, (1.0 - ratio) * 100.0, fds[i].values[1],
             fds[i].values[2]);
    } else {
      printf("%'20" PRIu64 " %s (%.2f%% scaling, ena=%'" PRIu64
             ", run=%'" PRIu64 ")\n",
             val, fds[i].name, (1.0 - ratio) * 100.0, fds[i].values[1],
             fds[i].values[2]);
    }

    fds[i].prev_values[0] = fds[i].values[0];
    fds[i].prev_values[1] = fds[i].values[1];
    fds[i].prev_values[2] = fds[i].values[2];
  }
}

int get_event_info(process_info_node_t* pid_list,int size, options_t* options) {
  perf_event_desc_t **fds_arr = (perf_event_desc_t **) malloc(sizeof(perf_event_desc_t *)*size);
  int *num_fds_arr = (int *) malloc(sizeof(int *) * size);
  int j, status, ret, i, num_fds = 0, grp, group_fd;
  //int ready[2], go[2];
  //char buf;
  pid_t pid;
  //go[0] = go[1] = -1;

  if (pfm_initialize() != PFM_SUCCESS) {
    errx(1, "libpfm initialization failed");
  }
  process_info_node_t* next = pid_list;
  for(j=0; j<size; j++) {
    printf("In the order of j %d, pid %d\n", j, next->process_id);
    perf_event_desc_t *fds=NULL;
    num_fds = 0;
    for (grp = 0; grp < options->num_groups; grp++) {
      ret = perf_setup_list_events(options->events[grp], &fds, &num_fds);
      if (ret || !num_fds) {
        exit(1);
      }
    }
    pid = next->process_id;
        for (i = 0; i < num_fds; i++) {
          int is_group_leader; /* boolean */

          is_group_leader = perf_is_group_leader(fds, i);
          if (is_group_leader) {
            /* this is the group leader */
            group_fd = -1;
          } else {
            group_fd = fds[fds[i].group_leader].fd;
          }

          /*
           * create leader disabled with enable_on-exec
           */
          fds[i].hw.disabled = is_group_leader;
          fds[i].hw.enable_on_exec = is_group_leader;

          fds[i].hw.read_format = PERF_FORMAT_SCALE;

          fds[i].fd = perf_event_open(&fds[i].hw, pid, -1, group_fd, 0);
          if (fds[i].fd == -1) {
            warn("cannot attach event%d %s", i, fds[i].name);
            goto error;
          }
        }
    fds_arr[j]=fds;
    num_fds_arr[j]=num_fds;
    next = next->next;
  }
  for (j=0; j<size; j++)
  {
    perf_event_desc_t *fds=fds_arr[j];
    num_fds=num_fds_arr[j];
        if (options->print) {
          while (todo_option_quit == 0) {
            sleep(1);
            print_counts(fds, num_fds);
          }
        } else {
          pause();
          printf("for the pid number %d\n",j);
          print_counts(fds, num_fds);
        }

        for (i = 0; i < num_fds; i++) {
          close(fds[i].fd);
        }
        perf_free_fds(fds, num_fds);
    
  }

  free(fds_arr);
  /* free libpfm resources cleanly */
  pfm_terminate();
  return 0;
error:
  //free(fds);

  /* free libpfm resources cleanly */
  pfm_terminate();

  return -1;
}
