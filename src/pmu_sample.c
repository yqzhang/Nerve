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

#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define MAX_LENGTH_PER_LINE 256

#define MICROSECONDS 1000000

#define CPU_CLK_UNHALTED_CORE 0x030a
#define CPU_CLK_UNHALTED_REF 0x030b

// Timestamp to correct the sampling interval
struct timeval si_tvs;
int sleep_offset;

// Total number of cores we need to monitor
int num_of_cores;

// Data structures that we need to monitor CPU frequency related events
struct timeval tvs[2];
unsigned long long cycles[2];
unsigned long long cpu_clk_unhalted_core[2][MAX_NUM_CORES];
unsigned long long cpu_clk_unhalted_ref[2][MAX_NUM_CORES];

// Data structures that we need to monitor PMU events
perf_event_desc_t* pmu_fds[MAX_NUM_PROCESSES];

// Data structures that we need to monitor interrupt handling
long long prev_interrupt_per_core[MAX_NUM_CORES];
long long interrupt_per_core[MAX_NUM_CORES];

// Data structures that we need to monitor network traffic
unsigned long long network_recv_bytes, prev_network_recv_bytes;
unsigned long long network_recv_packets, prev_network_recv_packets;
unsigned long long network_recv_errs, prev_network_recv_errs;
unsigned long long network_recv_drops, prev_network_recv_drops;
unsigned long long network_send_bytes, prev_network_send_bytes;
unsigned long long network_send_packets, prev_network_send_packets;
unsigned long long network_send_errs, prev_network_send_errs;
unsigned long long network_send_drops, prev_network_send_drops;

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

void get_irq_stats(long long interrupt_per_core[MAX_NUM_CORES]) {
  FILE* fp;
  char line[MAX_LENGTH_PER_LINE];
  long long local_interrupt_per_core[MAX_NUM_CORES];

  // Reset the counters
  int c;
  for (c = 0; c < num_of_cores; c++) {
    interrupt_per_core[c] = 0;
  }

  fp = fopen("/proc/interrupts", "r");
  if (fp == NULL) {
    logging(LOG_CODE_FATAL, "Unable to read /proc/interrupts.\n");
  }

  // Skip the first line
  fgets(line, MAX_LENGTH_PER_LINE, fp);
  while (fgets(line, MAX_LENGTH_PER_LINE, fp) != NULL) {
    // Check if the first column starts with numbers
    int i = 0;
    while (isspace(line[i])) i++;
    if (line[i] < '0' || line[i] > '9') {
      break;
    }
    while (!isspace(line[i])) i++;

    int j;
    for (j = 0; j < num_of_cores; j++) {
      int k = 0;
      char int_buffer[16];
      while (isspace(line[i])) i++;
      while (!isspace(line[i])) int_buffer[k++] = line[i++];
      int_buffer[k] = '\0';
      local_interrupt_per_core[j] = atoll(int_buffer);
    }

    // Skip the second last column
    while (isspace(line[i])) i++;
    while (!isspace(line[i])) i++;
    while (isspace(line[i])) i++;

    // Check if the last column starts with "eth"
    if (line[i] == 'e' && line[i + 1] == 't' && line[i + 2] == 'h') {
      for (j = 0; j < num_of_cores; j++) {
        interrupt_per_core[j] += local_interrupt_per_core[j];
      }
    }
  }

  fclose(fp);
}

void estimate_irq(long long irq_info[MAX_NUM_CORES]) {
  int i;
  for (i = 0; i < num_of_cores; i++) {
    irq_info[i] = interrupt_per_core[i] - prev_interrupt_per_core[i];
  }
}

void get_network_stats(unsigned long long* network_recv_bytes,
                       unsigned long long* network_recv_packets,
                       unsigned long long* network_recv_errs,
                       unsigned long long* network_recv_drops,
                       unsigned long long* network_send_bytes,
                       unsigned long long* network_send_packets,
                       unsigned long long* network_send_errs,
                       unsigned long long* network_send_drops) {
  // Reset all the counts before doing anything
  *network_recv_bytes = 0ULL;
  *network_recv_packets = 0ULL;
  *network_recv_errs = 0ULL;
  *network_recv_drops = 0ULL;
  *network_send_bytes = 0ULL;
  *network_send_packets = 0ULL;
  *network_send_errs = 0ULL;
  *network_send_drops = 0ULL;

  FILE* fp;
  char line[MAX_LENGTH_PER_LINE];

  fp = fopen("/proc/net/dev", "r");
  if (fp == NULL) {
    logging(LOG_CODE_FATAL, "Unable to read /proc/net/dev.\n");
  }

  // Skip the first 2 lines
  fgets(line, MAX_LENGTH_PER_LINE, fp);
  fgets(line, MAX_LENGTH_PER_LINE, fp);
  while (fgets(line,MAX_LENGTH_PER_LINE, fp) != NULL) {
    /*
     * We probably do not care about loop back, but all the other ethernet
     * cards might be interesting. So maybe just sum them up.
     *
     * File format:
     * (1) interface
     * (2) recv_bytes
     * (3) recv_packets
     * (4) recv_errs
     * (5) recv_drop
     * ...
     * (10) send_bytes
     * (11) send_packets
     * (12) send_errs
     * (13) send_drop
     */
    char interface[8];
    unsigned long long recv_bytes, recv_packets, recv_errs, recv_drops;
    unsigned long long send_bytes, send_packets, send_errs, send_drops;
    sscanf(line,
           "%s %llu %llu %llu %llu %*u %*u %*u %*u %llu %llu %llu %llu",
           interface, &recv_bytes, &recv_packets, &recv_errs, &recv_drops,
           &send_bytes, &send_packets, &send_errs, &send_drops);
    if (strncmp(interface, "eth", 3) == 0) {
      *network_recv_bytes += recv_bytes;
      *network_recv_packets += recv_packets;
      *network_recv_errs += recv_errs;
      *network_recv_drops += recv_drops;
      *network_send_bytes += send_bytes;
      *network_send_packets += send_packets;
      *network_send_errs += send_errs;
      *network_send_drops += send_drops;
    }
  }

  fclose(fp);
}

void estimate_network(unsigned long long network_info[8]) {
  network_info[0] = network_recv_bytes - prev_network_recv_bytes;
  network_info[1] = network_recv_packets - prev_network_recv_packets;
  network_info[2] = network_recv_errs - prev_network_recv_errs;
  network_info[3] = network_recv_drops - prev_network_recv_drops;
  network_info[4] = network_send_bytes - prev_network_send_bytes;
  network_info[5] = network_send_packets - prev_network_send_packets;
  network_info[6] = network_send_errs - prev_network_send_errs;
  network_info[7] = network_send_drops - prev_network_send_drops;
}

uint64_t read_msr(int cpu, uint32_t reg, unsigned int highbit,
                  unsigned int lowbit) {
  uint64_t data;
  int fd;
  char msr_file_name[64];

  sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
  fd = open(msr_file_name, O_RDONLY);
  if (fd < 0) {
    logging(LOG_CODE_FATAL,
            "Cannot read MSR files (root permission required).\n");
  }

  if (pread(fd, &data, sizeof(data), reg) != sizeof(data)) {
    logging(LOG_CODE_FATAL,
            "Error reading MSR files.\n");
  }

  close(fd);

  int bits = highbit - lowbit + 1;
  // Get the specific bits
  if (bits < 64) {
    data >>= lowbit;
    data &= (1ULL << bits) - 1;
  }

  // Make sure we get the correct sign here
  if (data & (1ULL << (bits - 1))) {
    data &= ~(1ULL << (bits - 1));
    data = -data;
  }

  return data;
}

void get_cpu_cycles(unsigned long long* cycles, struct timeval* tvs,
                    unsigned long long* cpu_clk_unhalted_core,
                    unsigned long long* cpu_clk_unhalted_ref) {
  int i;

  // Record the time
  gettimeofday(tvs, NULL);
  // Get the cycle count
  *cycles = rdtsc();

  for (i = 0; i < num_of_cores; i++) {
    cpu_clk_unhalted_core[i] = read_msr(i, CPU_CLK_UNHALTED_CORE, 63, 0);
    cpu_clk_unhalted_ref[i] = read_msr(i, CPU_CLK_UNHALTED_REF, 63, 0);
  }
}

void estimate_frequency(unsigned int frequency_info[MAX_NUM_CORES]) {
  int i;

  unsigned int microseconds =
      (tvs[1].tv_sec - tvs[0].tv_sec) * MICROSECONDS +
      (tvs[1].tv_usec - tvs[0].tv_usec);

  unsigned int total_freq = 0;
  int total_cores = 0;

  // FIXME: There seems to be some issues reading MSR on SandyBridge when
  // SMT is enabled, maybe we should instead use the numbers we get from the
  // actual logical cores.
  for (i = 0; i < num_of_cores; i++) {
    unsigned long long delta_cpu_clk_unhalted_core, delta_cpu_clk_unhalted_ref;
    // Handle overflow for CPU_CLK_UNHALTED_CORE
    if (cpu_clk_unhalted_core[1][i] < cpu_clk_unhalted_core[0][i]) {
      delta_cpu_clk_unhalted_core =
          (UINT64_MAX - cpu_clk_unhalted_core[0][i]) +
          cpu_clk_unhalted_core[1][i];
    } else {
      delta_cpu_clk_unhalted_core =
          cpu_clk_unhalted_core[1][i] - cpu_clk_unhalted_core[0][i];
    }
    // Handle overflow for CPU_CLK_UNHALTED_REF
    if (cpu_clk_unhalted_ref[1][i] < cpu_clk_unhalted_ref[0][i]) {
      delta_cpu_clk_unhalted_ref =
          (UINT64_MAX - cpu_clk_unhalted_ref[0][i]) +
          cpu_clk_unhalted_ref[1][i];
    } else {
      delta_cpu_clk_unhalted_ref =
          cpu_clk_unhalted_ref[1][i] - cpu_clk_unhalted_ref[0][i];
    }
    unsigned int frequency =
        ((cycles[1] - cycles[0]) / microseconds) *
        ((double) delta_cpu_clk_unhalted_core /
         (double) delta_cpu_clk_unhalted_ref);

    // FIXME: sometimes cpu_clk_unhalted_core shows weird numbers
    if (frequency > 4000) {
      frequency_info[i] = 0;
    } else {
      frequency_info[i] = frequency;
      total_freq += frequency;
      total_cores++;
    }
  }

  unsigned int avg_frequency = total_freq / total_cores;
  for (i = 0; i < num_of_cores; i++) {
    if (frequency_info[i] == 0) {
      frequency_info[i] = avg_frequency;
    }
  }
}

void init_pmu_sample(hardware_info_t* hardware_info) {
  // Get the total number of cores available
  num_of_cores = sysconf(_SC_NPROCESSORS_ONLN);
  hardware_info->num_of_cores = num_of_cores;

  // Initialize libpfm
  int ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    logging(LOG_CODE_FATAL, "Cannot initialize library: %s", pfm_strerror(ret));
  }

  // Initialize the sample interval timestamp
  gettimeofday(&si_tvs, NULL);
  sleep_offset = 9000;
}

void clean_pmu_sample() {
  /* free libpfm resources cleanly */
  pfm_terminate();
}

void record_pmu_sample(
         perf_event_desc_t** fds, int num_fds, int num_pmus,
         unsigned long long pmu_info[MAX_NUM_PROCESSES][MAX_EVENTS],
         int child_thread_mapping[MAX_NUM_PROCESSES * MAX_NUM_THREADS]) {
  uint64_t val;
  uint64_t values[3];
  int fds_index, pmu_index;
  ssize_t ret;

  // Reset the values
  memset(pmu_info, 0, MAX_NUM_PROCESSES * MAX_NUM_THREADS * sizeof(int));

  /*
   * now read the results. We use pfp_event_count because
   * libpfm guarantees that counters for the events always
   * come first.
   */
  for (pmu_index = 0; pmu_index < num_pmus; pmu_index++) {
    memset(values, 0, sizeof(values));

    for (fds_index = 0; fds_index < num_fds; fds_index++) {
      ret = read(fds[pmu_index][fds_index].fd, values, sizeof(values));
      if (ret < (ssize_t)sizeof(values)) {
        if (ret == -1) {
          // FIXME: the corresponding process has already gone
          // logging(LOG_CODE_WARNING, "cannot read results: %s", "strerror(errno)");
          val = 0;
        } else {
          logging(LOG_CODE_WARNING, "could not read event %d", fds_index);
        }
      } else {
        /*
         * scaling is systematic because we may be sharing the PMU and
         * thus may be multiplexed
         */
        val = perf_scale(values);
      }

      pmu_info[child_thread_mapping[pmu_index]][fds_index] += val;
    }
  }
}

void get_pmu_sample(process_list_t* process_info_list,
                    const char* events[MAX_EVENTS],
                    unsigned int sample_interval,
                    hardware_info_t* hardware_info) {
  int pmu_index, num_pmus;
  int fds_index, num_fds;
  int proc_index;
  int ret;
  int child_thread_mapping[MAX_NUM_PROCESSES * MAX_NUM_THREADS];

  /*
   * Initialize pfm library (required before we can use it)
   */
  pmu_index = 0;
  for (proc_index = 0; proc_index < process_info_list->size; proc_index++) {
    // Handle all the threads, including parent and child
    int i;
    for (i = 0;
         i < process_info_list->processes_i[proc_index].child_thread_ids_size;
         i++) {
      pmu_fds[pmu_index] = NULL;
      ret = perf_setup_argv_events(events, &pmu_fds[pmu_index], &num_fds);
      if (ret || !num_fds) {
        logging(LOG_CODE_FATAL, "cannot setup events");
      }

      pmu_fds[pmu_index][0].fd = -1;

      for (fds_index = 0; fds_index < num_fds; fds_index++) {
        /* request timing information necessary for scaling */
        pmu_fds[pmu_index][fds_index].hw.read_format = PERF_FORMAT_SCALE;
        pmu_fds[pmu_index][fds_index].hw.inherit = 1;
        pmu_fds[pmu_index][fds_index].hw.disabled = 1; /* do no start now */
        /* each event is in an independent group (multiplexing likely) */
        pmu_fds[pmu_index][fds_index].fd = perf_event_open(
            &pmu_fds[pmu_index][fds_index].hw,
            process_info_list->processes_i[proc_index].child_thread_ids[i],
            -1, -1, 0);
        // TODO: The corresponding process has already gone
        if (pmu_fds[pmu_index][fds_index].fd == -1) {
          // logging(LOG_CODE_WARNING, "cannot open event %d, errno: %s\n", fds_index, strerror(errno));
        }
      }
      // Record the parent thread
      child_thread_mapping[pmu_index] = proc_index;
      // Increment the PMU index for each child thread
      pmu_index++;
    }
  }

  // Total number of PMUs
  num_pmus = pmu_index;

  /*
   * enable all counters attached to this thread and created by it
   */
  ret = prctl(PR_TASK_PERF_EVENTS_ENABLE);
  if (ret) {
    logging(LOG_CODE_FATAL, "prctl(enable) failed");
  }

  // Network interrupt handling
  get_irq_stats(prev_interrupt_per_core);
  // CPU frequency
  get_cpu_cycles(&cycles[0], &tvs[0], cpu_clk_unhalted_core[0],
                 cpu_clk_unhalted_ref[0]);
  // Network
  get_network_stats(&prev_network_recv_bytes, &prev_network_recv_packets,
                    &prev_network_recv_errs, &prev_network_recv_drops,
                    &prev_network_send_bytes, &prev_network_send_packets,
                    &prev_network_send_errs, &prev_network_send_drops);

  // Sample interval controller
  // usleep(sample_interval - sleep_offset);
  usleep(sample_interval);

  // Correct the amount of time we need to sleep
  struct timeval curr_tvs;
  gettimeofday(&curr_tvs, NULL);
  // sleep_offset = (curr_tvs.tv_sec - si_tvs.tv_sec) * MICROSECONDS +
  //     (curr_tvs.tv_usec - si_tvs.tv_usec) - sample_interval;
  si_tvs = curr_tvs;

  // logging(LOG_CODE_INFO, "offset: %d\n", sleep_offset);

  // Network interrupt handling
  get_irq_stats(interrupt_per_core);
  estimate_irq(hardware_info->irq_info);
  // CPU frequency
  get_cpu_cycles(&cycles[1], &tvs[1], cpu_clk_unhalted_core[1],
                 cpu_clk_unhalted_ref[1]);
  estimate_frequency(hardware_info->frequency_info);
  // Network
  get_network_stats(&network_recv_bytes, &network_recv_packets,
                    &network_recv_errs, &network_recv_drops,
                    &network_send_bytes, &network_send_packets,
                    &network_send_errs, &network_send_drops);
  estimate_network(hardware_info->network_info);

  record_pmu_sample(pmu_fds, num_fds, num_pmus,
                    hardware_info->pmu_info, child_thread_mapping);

  /*
   * disable all counters attached to this thread
   */
  ret = prctl(PR_TASK_PERF_EVENTS_DISABLE);
  if (ret) logging(LOG_CODE_FATAL, "prctl(disable) failed");

  for (pmu_index = 0; pmu_index < num_pmus; pmu_index++) {
    for (fds_index = 0; fds_index < num_fds; fds_index++) {
      close(pmu_fds[pmu_index][fds_index].fd);
    }
    perf_free_fds(pmu_fds[pmu_index], num_fds);
  }
}
