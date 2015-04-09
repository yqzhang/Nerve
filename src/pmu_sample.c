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

#include "log_util.h"

#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MAX_NUM_CORES 40

#define MICROSECONDS 1000000

int num_of_cores;
cpu_set_t all_core_mask;

unsigned long long cycles[2][MAX_NUM_CORES];

struct timeval tvs[2][MAX_NUM_CORES];

// This function reads the raw cycle count on a core, which depends on the
// core that this process is running on. Depending on the underlying
// architecture, the implementation varies:
// http://www.mcs.anl.gov/~kazutomo/rdtsc.html
#if defined(__i386__)
__inline__ unsigned long long rdtsc() {
  unsigned long long x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}

#elif defined(__x86_64__)
__inline__ unsigned long long rdtsc(void) {
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#elif defined(__powerpc__)
__inline__ unsigned long long rdtsc(void) {
  unsigned long long int result = 0;
  unsigned long int upper, lower, tmp;
  __asm__ volatile(
                "0:\n"
                "\tmftbu   %0\n"
                "\tmftb    %1\n"
                "\tmftbu   %2\n"
                "\tcmpw    %2,%0\n"
                "\tbne     0b\n"
                : "=r" (upper), "=r" (lower), "=r" (tmp)
                );
  result = upper;
  result = result << 32;
  result = result | lower;

  return result;
}
#endif

void init_pmu_sample() {
  // Get the total number of cores available
  num_of_cores = sysconf(_SC_NPROCESSORS_ONLN);

  // Prepare the mask for setting the process to all the cores
  CPU_ZERO(&all_core_mask);
  int i;
  for (i = 0; i < num_of_cores; i++) {
    CPU_SET(i, &all_core_mask);
  }
}

void get_cpu_cycles(unsigned long long* cycles, struct timeval* tvs) {
  int i;
  cpu_set_t mask;

  for (i = 0; i < num_of_cores; i++) {
    // Re-schedule the process to core i
    CPU_ZERO(&mask);
    CPU_SET(i, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
    // Record the time
    gettimeofday(&tvs[i], NULL);
    // Get the cycle count
    cycles[i] = rdtsc();
  }

  // Reset the affinity back to free
  sched_setaffinity(0, sizeof(cpu_set_t), &all_core_mask);
}

void estimate_frequency() {
  int i;

  for (i = 0; i < num_of_cores; i++) {
    unsigned int microseconds =
        (tvs[1][i].tv_sec - tvs[0][i].tv_sec) * MICROSECONDS +
        (tvs[1][i].tv_usec - tvs[0][i].tv_usec);
    unsigned int frequency =
        (cycles[1][i] - cycles[0][i]) / microseconds;
    logging(LOG_CODE_INFO, "Core %d: frequency %uMHz\n", i, frequency);
  }
}

void read_groups(perf_event_desc_t* fds, int num) {
  uint64_t* values = NULL;
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
      logging(LOG_CODE_FATAL, "cannot allocate memory for values\n");
    }

    ret = read(fds[evt].fd, values, new_sz);
    if (ret != (ssize_t)new_sz) {/* unsigned */
      if (ret == -1) {
        logging(LOG_CODE_FATAL, "cannot read values event %s", fds[evt].name);
      }

      /* likely pinned and could not be loaded */
      logging(LOG_CODE_WARNING,
              "could not read event %d, tried to read %zu bytes, but got %zd",
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

void print_pmu_sample(perf_event_desc_t** fds, int num_fds, int num_procs,
                      uint64_t proc_info[][20]) {
  uint64_t val;
  uint64_t values[3];
  double ratio;
  int fds_index, proc_index;
  ssize_t ret;

  /*
   * now read the results. We use pfp_event_count because
   * libpfm guarantees that counters for the events always
   * come first.
   */
  for (proc_index = 0; proc_index < num_procs; proc_index++) {
    memset(values, 0, sizeof(values));

    for (fds_index = 0; fds_index < num_fds; fds_index++) {
      ret = read(fds[proc_index][fds_index].fd, values, sizeof(values));
      if (ret < (ssize_t)sizeof(values)) {
        if (ret == -1) {
          logging(LOG_CODE_FATAL, "cannot read results: %s", "strerror(errno)");
        } else {
          logging(LOG_CODE_WARNING, "could not read event%d", fds_index);
        }
      }
      /*
       * scaling is systematic because we may be sharing the PMU and
       * thus may be multiplexed
       */
      val = perf_scale(values);
      ratio = perf_scale_ratio(values);

      printf("%'20" PRIu64 " %s (%.2f%% scaling, raw=%'" PRIu64
             ", ena=%'" PRIu64 ", run=%'" PRIu64 ")\n",
             val, fds[0][fds_index].name, (1.0 - ratio) * 100.0, values[0],
             values[1], values[2]);
      proc_info[proc_index][fds_index] = val;
    }
  }
}

void get_pmu_sample(process_list_t* process_info_list,
                    const char* events[MAX_GROUPS],
                    unsigned int sample_interval) {
  perf_event_desc_t* fds[512];
  int fds_index, proc_index, ret, num_fds;
  uint64_t proc_info[512][20];  // The data structure for storing the count.

  /*
   * Initialize pfm library (required before we can use it)
   */
  ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    logging(LOG_CODE_FATAL, "Cannot initialize library: %s", pfm_strerror(ret));
  }

  for (proc_index = 0; proc_index < process_info_list->size; proc_index++) {
    proc_info[proc_index][0] =
        process_info_list->processes[proc_index].process_id;
    fds[proc_index] = NULL;
    ret = perf_setup_argv_events(events, &(fds[proc_index]), &num_fds);
    if (ret || !num_fds) {
      logging(LOG_CODE_FATAL, "cannot setup events");
    }

    fds[proc_index][0].fd = -1;

    for (fds_index = 0; fds_index < num_fds; fds_index++) {
      /* request timing information necessary for scaling */
      fds[proc_index][fds_index].hw.read_format = PERF_FORMAT_SCALE;
      fds[proc_index][fds_index].hw.disabled = 1; /* do not start now */
      /* each event is in an independent group (multiplexing likely) */
      fds[proc_index][fds_index].fd = perf_event_open(
          &fds[proc_index][fds_index].hw, proc_info[proc_index][0], -1, -1, 0);
      if (fds[proc_index][fds_index].fd == -1) {
        logging(LOG_CODE_FATAL, "cannot open event %d", fds_index);
      }
    }
  }

  /*
   * enable all counters attached to this thread and created by it
   */
  ret = prctl(PR_TASK_PERF_EVENTS_ENABLE);
  if (ret) {
    logging(LOG_CODE_FATAL, "prctl(enable) failed");
  }

  get_cpu_cycles(cycles[0], tvs[0]);

  usleep(sample_interval);

  get_cpu_cycles(cycles[1], tvs[1]);
  estimate_frequency();

  print_pmu_sample(fds, num_fds, process_info_list->size, proc_info);

  /*
   * disable all counters attached to this thread
   */
  ret = prctl(PR_TASK_PERF_EVENTS_DISABLE);
  if (ret) logging(LOG_CODE_FATAL, "prctl(disable) failed");

  for (proc_index = 0; proc_index < process_info_list->size; ++proc_index) {
    for (fds_index = 0; fds_index < num_fds; fds_index++)
      close(fds[proc_index][fds_index].fd);

    perf_free_fds(fds[proc_index], num_fds);
  }

  /* free libpfm resources cleanly */
  pfm_terminate();
}
