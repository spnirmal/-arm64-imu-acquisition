#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "iio-device.hpp"
#include "version.hpp"

#define DEFAULT_TOTAL_SAMPLES 100000

struct SampleRecord {
  uint64_t sample_index;
  struct timespec irq_ts;       // kernel IIO buffer monotonic timestamp (at GPIO IRQ)
  struct timespec post_read_ts; // clock_gettime(CLOCK_MONOTONIC) post read()
  float ax;
  float ay;
  float az;
};

static volatile sig_atomic_t g_running = 1;

static void sig_handler(int sig) {
  (void)sig;
  g_running = 0;
}

static bool dump_to_csv(const char *filepath, const std::vector<SampleRecord> &samples, size_t count) {
  printf("Dumping %zu samples to %s...\n", count, filepath);
  FILE *fp = fopen(filepath, "w");
  if (!fp) {
    perror("Failed to open CSV file for writing");
    return false;
  }

  // Use a 64KB write buffer for fast I/O throughput
  char io_buffer[65536];
  setvbuf(fp, io_buffer, _IOFBF, sizeof(io_buffer));

  // CSV Header matching Arch-A schema
  fprintf(fp, "sample_index,irq_sec,irq_nsec,post_read_sec,post_read_nsec,latency_ns,ax_g,ay_g,az_g\n");

  int64_t min_lat = -1;
  int64_t max_lat = -1;
  double sum_lat = 0.0;

  for (size_t i = 0; i < count; ++i) {
    const SampleRecord &s = samples[i];
    int64_t diff_sec = (int64_t)s.post_read_ts.tv_sec - (int64_t)s.irq_ts.tv_sec;
    int64_t diff_nsec = (int64_t)s.post_read_ts.tv_nsec - (int64_t)s.irq_ts.tv_nsec;
    int64_t latency_ns = diff_sec * 1000000000LL + diff_nsec;

    if (min_lat < 0 || latency_ns < min_lat) min_lat = latency_ns;
    if (max_lat < 0 || latency_ns > max_lat) max_lat = latency_ns;
    sum_lat += (double)latency_ns;

    fprintf(fp, "%lu,%ld,%ld,%ld,%ld,%lld,%.4f,%.4f,%.4f\n",
            (unsigned long)s.sample_index,
            (long)s.irq_ts.tv_sec,
            (long)s.irq_ts.tv_nsec,
            (long)s.post_read_ts.tv_sec,
            (long)s.post_read_ts.tv_nsec,
            (long long)latency_ns,
            s.ax,
            s.ay,
            s.az);
  }

  fclose(fp);
  printf("Successfully dumped %zu samples to %s\n", count, filepath);

  if (count > 0) {
    printf("Latency Statistics (Kernel IIO IRQ -> Post-Read Transfer):\n");
    printf("  Min Latency : %.2f us\n", (double)min_lat / 1000.0);
    printf("  Max Latency : %.2f us\n", (double)max_lat / 1000.0);
    printf("  Mean Latency: %.2f us\n", (sum_lat / (double)count) / 1000.0);
  }

  return true;
}

int main(int argc, char *argv[]) {
  printf("%s version %s\n", APP_NAME, APP_VERSION);
  fflush(stdout);

  const char *csv_output = "iio_acquisition_100k.csv";
  size_t total_samples = DEFAULT_TOTAL_SAMPLES;
  std::string dev_name = "mpu9250";

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      return 0;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      csv_output = argv[++i];
    } else if (strncmp(argv[i], "--output=", 9) == 0) {
      csv_output = argv[i] + 9;
    } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
      total_samples = (size_t)atol(argv[++i]);
    } else if (strncmp(argv[i], "--samples=", 10) == 0) {
      total_samples = (size_t)atol(argv[i] + 10);
    } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
      dev_name = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [options] [output_csv]\n", argv[0]);
      printf("Options:\n");
      printf("  -o, --output <file>    Specify CSV output file path (default: iio_acquisition_100k.csv)\n");
      printf("  -n, --samples <count>  Number of samples to collect (default: %d)\n", DEFAULT_TOTAL_SAMPLES);
      printf("  -d, --device <name>    Preferred IIO device name (default: mpu9250)\n");
      printf("  -v, --version          Show version and exit\n");
      printf("  -h, --help             Show help message and exit\n");
      return 0;
    } else if (argv[i][0] != '-') {
      csv_output = argv[i];
    }
  }

  // Pre-allocate memory array for samples
  std::vector<SampleRecord> samples(total_samples);

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  iio_device sensor(dev_name);
  if (!sensor.init()) {
    fprintf(stderr, "Failed to initialize IIO sensor\n");
    return 1;
  }

  printf("Pre-allocated array for %zu samples. Starting IIO stream acquisition...\n", total_samples);
  fflush(stdout);

  size_t samples_collected = 0;
  while (g_running && samples_collected < total_samples) {
    IioSample s = {};
    if (sensor.read_sample(s, 1000)) {
      SampleRecord &sample = samples[samples_collected];
      sample.sample_index = samples_collected;
      sample.irq_ts = s.kernel_ts;
      sample.post_read_ts = s.post_read_ts;
      sample.ax = s.ax;
      sample.ay = s.ay;
      sample.az = s.az;
      samples_collected++;
    } else if (!g_running) {
      break;
    }
  }

  printf("\nAcquisition loop finished. Total samples collected: %zu\n", samples_collected);
  fflush(stdout);

  sensor.close_device();

  dump_to_csv(csv_output, samples, samples_collected);

  return 0;
}
