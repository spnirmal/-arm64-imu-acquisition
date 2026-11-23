#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "gpio-layer.hpp"
#include "mpu9250.hpp"
#include "spi-device.hpp"
#include "version.hpp"

#define TOTAL_SAMPLES 100000
#define SPEED 1000000 // 1 MHz
#define BYTES 3

int bytes = BYTES;
int speed = SPEED;

struct SampleRecord {
  uint64_t sample_index;
  struct timespec irq_ts;      // kernel gpio irq monotonic timestamp from event.ts
  struct timespec post_spi_ts; // clock_gettime(CLOCK_MONOTONIC) post spiXfer
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

  // CSV Header
  fprintf(fp, "sample_index,irq_sec,irq_nsec,post_spi_sec,post_spi_nsec,latency_ns,ax_g,ay_g,az_g\n");

  int64_t min_lat = -1;
  int64_t max_lat = -1;
  double sum_lat = 0.0;

  for (size_t i = 0; i < count; ++i) {
    const SampleRecord &s = samples[i];
    int64_t diff_sec = (int64_t)s.post_spi_ts.tv_sec - (int64_t)s.irq_ts.tv_sec;
    int64_t diff_nsec = (int64_t)s.post_spi_ts.tv_nsec - (int64_t)s.irq_ts.tv_nsec;
    int64_t latency_ns = diff_sec * 1000000000LL + diff_nsec;

    if (min_lat < 0 || latency_ns < min_lat) min_lat = latency_ns;
    if (max_lat < 0 || latency_ns > max_lat) max_lat = latency_ns;
    sum_lat += (double)latency_ns;

    fprintf(fp, "%lu,%ld,%ld,%ld,%ld,%lld,%.4f,%.4f,%.4f\n",
            (unsigned long)s.sample_index,
            (long)s.irq_ts.tv_sec,
            (long)s.irq_ts.tv_nsec,
            (long)s.post_spi_ts.tv_sec,
            (long)s.post_spi_ts.tv_nsec,
            (long long)latency_ns,
            s.ax,
            s.ay,
            s.az);
  }

  fclose(fp);
  printf("Successfully dumped %zu samples to %s\n", count, filepath);

  if (count > 0) {
    printf("Latency Statistics (Kernel IRQ -> Post-SPI Transfer):\n");
    printf("  Min Latency : %.2f us\n", (double)min_lat / 1000.0);
    printf("  Max Latency : %.2f us\n", (double)max_lat / 1000.0);
    printf("  Mean Latency: %.2f us\n", (sum_lat / (double)count) / 1000.0);
  }

  return true;
}

int main(int argc, char *argv[]) {
  // Print version string as the first thing upon running
  printf("%s version %s\n", APP_NAME, APP_VERSION);
  fflush(stdout);

  const char *csv_output = "spi_acquisition_100k.csv";

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      return 0;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      csv_output = argv[++i];
    } else if (strncmp(argv[i], "--output=", 9) == 0) {
      csv_output = argv[i] + 9;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [options] [output_csv]\n", argv[0]);
      printf("Options:\n");
      printf("  -o, --output <file>  Specify CSV output file path (default: spi_acquisition_100k.csv)\n");
      printf("  -v, --version        Show version and exit\n");
      printf("  -h, --help           Show help message and exit\n");
      return 0;
    } else if (argv[i][0] != '-') {
      csv_output = argv[i];
    }
  }

  // Pre-allocate memory array for 100,000 samples (~5.6 MB)
  std::vector<SampleRecord> samples(TOTAL_SAMPLES);

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  gpio accel_int_gpio(ACCEL_INT_GPIO);
  if (accel_int_gpio.configure() < 0) {
    fprintf(stderr, "Failed to configure GPIO interrupt\n");
    return 1;
  }

  mpu9250 accelerometer(speed);

  printf("Configured SPI at %d Hz (1 MHz), MPU-9250 ODR at 1000 Hz\n", speed);
  printf("Pre-allocated array for %d samples. Starting acquisition...\n", TOTAL_SAMPLES);
  fflush(stdout);

  size_t samples_collected = 0;
  while (g_running && samples_collected < TOTAL_SAMPLES) {
    struct timespec irq_ts = {};
    int ret = accel_int_gpio.wait_event(&irq_ts, 1000);
    if (ret > 0) {
      SampleRecord &sample = samples[samples_collected];
      sample.sample_index = samples_collected;
      sample.irq_ts = irq_ts;
      if (accelerometer.read_accel(&sample.ax, &sample.ay, &sample.az, &sample.post_spi_ts)) {
        samples_collected++;
      }
    } else if (ret < 0) {
      if (!g_running) break;
      perror("wait_event error");
      break;
    }
  }

  printf("\nAcquisition loop finished. Total samples collected: %zu\n", samples_collected);
  fflush(stdout);

  dump_to_csv(csv_output, samples, samples_collected);

  return 0;
}
