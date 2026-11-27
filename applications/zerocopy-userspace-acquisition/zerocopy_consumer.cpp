#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#define APP_NAME "zerocopy-consumer"
#define APP_VERSION "1.0.0"
#define DEFAULT_TOTAL_SAMPLES 100000
#define RING_SLOTS 62
#define ACCEL_SCALE (1.0f / 16384.0f) // ±2g: 16384 LSB/g
#define GYRO_SCALE (1.0f / 131.0f)    // ±250 dps: 131 LSB/(deg/s)

struct __attribute__((aligned(64))) imu_frame {
  uint64_t hw_timestamp_ns;
  uint32_t seq_num;
  int16_t accel[3];
  int16_t gyro[3];
  uint8_t reserved[40];
};

struct __attribute__((aligned(64))) imu_ring_page {
  uint64_t head;
  uint64_t dropped_frames;
  uint8_t pad0[48];
  uint64_t tail;
  uint8_t pad1[56];
  struct imu_frame slots[RING_SLOTS];
};

struct SampleRecord {
  uint64_t sample_index;
  uint64_t hw_timestamp_ns;     // kernel top-half timestamp (ktime_get_ns)
  struct timespec post_read_ts; // clock_gettime(CLOCK_MONOTONIC) in userspace
  int64_t latency_ns;           // post_read_ns - hw_timestamp_ns
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
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

  // CSV Header matching Arch-A and Arch-B schema
  fprintf(fp, "sample_index,irq_sec,irq_nsec,post_read_sec,post_read_nsec,latency_ns,ax_g,ay_g,az_g\n");

  int64_t min_lat = -1;
  int64_t max_lat = -1;
  double sum_lat = 0.0;

  for (size_t i = 0; i < count; ++i) {
    const SampleRecord &s = samples[i];
    uint64_t irq_sec = s.hw_timestamp_ns / 1000000000ULL;
    uint64_t irq_nsec = s.hw_timestamp_ns % 1000000000ULL;

    if (min_lat < 0 || s.latency_ns < min_lat) min_lat = s.latency_ns;
    if (max_lat < 0 || s.latency_ns > max_lat) max_lat = s.latency_ns;
    sum_lat += (double)s.latency_ns;

    fprintf(fp, "%lu,%lu,%lu,%ld,%ld,%lld,%.4f,%.4f,%.4f\n",
            (unsigned long)s.sample_index,
            (unsigned long)irq_sec,
            (unsigned long)irq_nsec,
            (long)s.post_read_ts.tv_sec,
            (long)s.post_read_ts.tv_nsec,
            (long long)s.latency_ns,
            s.ax,
            s.ay,
            s.az);
  }

  fclose(fp);
  printf("Successfully dumped %zu samples to %s\n", count, filepath);

  if (count > 0) {
    printf("Latency Statistics (Kernel Hard IRQ -> Userspace Zero-Copy Consumer):\n");
    printf("  Min Latency : %.2f us\n", (double)min_lat / 1000.0);
    printf("  Max Latency : %.2f us\n", (double)max_lat / 1000.0);
    printf("  Mean Latency: %.2f us\n", (sum_lat / (double)count) / 1000.0);
  }

  return true;
}

int main(int argc, char *argv[]) {
  printf("%s version %s\n", APP_NAME, APP_VERSION);
  fflush(stdout);

  const char *csv_output = "zerocopy_acquisition_100k.csv";
  size_t total_samples = DEFAULT_TOTAL_SAMPLES;
  const char *device_path = "/dev/imu_zerocopy";

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
      device_path = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [options] [output_csv]\n", argv[0]);
      printf("Options:\n");
      printf("  -o, --output <file>    Specify CSV output file path (default: zerocopy_acquisition_100k.csv)\n");
      printf("  -n, --samples <count>  Number of samples to collect (default: %d)\n", DEFAULT_TOTAL_SAMPLES);
      printf("  -d, --device <path>    Device node path (default: /dev/imu_zerocopy)\n");
      printf("  -v, --version          Show version and exit\n");
      printf("  -h, --help             Show help message and exit\n");
      return 0;
    } else if (argv[i][0] != '-') {
      csv_output = argv[i];
    }
  }

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  // Attempt to elevate to real-time priority (SCHED_FIFO)
  struct sched_param sp;
  sp.sched_priority = 50;
  if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) {
    printf("Elevated consumer to SCHED_FIFO (priority %d)\n", sp.sched_priority);
  }

  int fd = open(device_path, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "Cannot open %s: %s\n", device_path, strerror(errno));
    fprintf(stderr, "Is the 'imu_zerocopy' kernel module loaded?\n");
    return 1;
  }

  auto *ring = (struct imu_ring_page *)mmap(
      nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ring == MAP_FAILED) {
    perror("mmap failed");
    close(fd);
    return 1;
  }

  printf("Mapped zero-copy SPSC ring page (4096 bytes, %d slots) at %p\n", RING_SLOTS, ring);

  std::vector<SampleRecord> samples;
  samples.resize(total_samples);
  size_t sample_count = 0;

  printf("Pre-allocated array for %zu samples. Starting zero-copy acquisition...\n", total_samples);

  struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};
  uint64_t tail = __atomic_load_n(&ring->tail, __ATOMIC_RELAXED);

  while (g_running && sample_count < total_samples) {
    /* ARM64 LDAR: Acquire payload writes before reading slot */
    uint64_t head = __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE);
    if (tail == head) {
      int pret = poll(&pfd, 1, 1000);
      if (pret < 0 && errno == EINTR) {
        break;
      }
      continue;
    }

    while (tail != head && sample_count < total_samples && g_running) {
      struct imu_frame *f = &ring->slots[tail % RING_SLOTS];
      struct timespec ts_user;
      clock_gettime(CLOCK_MONOTONIC, &ts_user);

      uint64_t user_ts_ns = (uint64_t)ts_user.tv_sec * 1000000000ULL + ts_user.tv_nsec;
      int64_t latency = (int64_t)(user_ts_ns - f->hw_timestamp_ns);

      SampleRecord &rec = samples[sample_count];
      rec.sample_index = sample_count;
      rec.hw_timestamp_ns = f->hw_timestamp_ns;
      rec.post_read_ts = ts_user;
      rec.latency_ns = latency;
      rec.ax = (float)f->accel[0] * ACCEL_SCALE;
      rec.ay = (float)f->accel[1] * ACCEL_SCALE;
      rec.az = (float)f->accel[2] * ACCEL_SCALE;
      rec.gx = (float)f->gyro[0] * GYRO_SCALE;
      rec.gy = (float)f->gyro[1] * GYRO_SCALE;
      rec.gz = (float)f->gyro[2] * GYRO_SCALE;

      sample_count++;
      tail++;

      /* ARM64 STLR: Release slot back to kernel producer */
      __atomic_store_n(&ring->tail, tail, __ATOMIC_RELEASE);
    }
  }

  printf("\nAcquisition loop finished. Total samples collected: %zu\n", sample_count);
  printf("Kernel ring buffer dropped frames: %llu\n", (unsigned long long)ring->dropped_frames);

  dump_to_csv(csv_output, samples, sample_count);

  munmap(ring, 4096);
  close(fd);
  return 0;
}
