#include "MPU9250.hpp"
#include "gpio-layer.hpp"
#include "spi-layer.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <mutex>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

#define DEFAULT_SPEED 100000
#define DEFAULT_GPIO_CHIP "gpiochip0"
#define DEFAULT_GPIO_PIN 17 // BCM GPIO 17 (Pin 11 on Raspberry Pi header)
#define DEFAULT_DISPLAY_INTERVAL_MS 500

char RXBuf[MAX_SPI_BUFSIZ];
char TXBuf[MAX_SPI_BUFSIZ];
int speed = DEFAULT_SPEED;

struct ProgramConfig {
  const char *chip_name = DEFAULT_GPIO_CHIP;
  unsigned int gpio_pin = DEFAULT_GPIO_PIN;
  int display_interval_ms = DEFAULT_DISPLAY_INTERVAL_MS;
  bool newline_mode = false;
};

// Thread-safe telemetry snapshot shared between acquisition and display threads
struct TelemetrySnapshot {
  std::mutex mtx;
  struct mpu9250_accel_data latest_accel{};
  uint64_t sample_count{0};
  bool has_data{false};
};

static volatile sig_atomic_t g_running = 1;
static ProgramConfig g_config;
static TelemetrySnapshot g_telemetry;
static std::condition_variable g_display_cv;
static std::mutex g_display_cv_mtx;
static std::chrono::steady_clock::time_point g_start_time;

static void sig_handler(int sig) {
  (void)sig;
  g_running = 0;
}

static void print_usage(const char *prog) {
  printf("Usage: %s [options]\n", prog);
  printf("Options:\n");
  printf("  -i, --interval <ms>   Screen update interval in milliseconds (default: %d)\n",
         DEFAULT_DISPLAY_INTERVAL_MS);
  printf("  -n, --newline         Print new line per update instead of in-place carriage return (\\r)\n");
  printf("  -s, --speed <hz>      SPI clock speed in Hz (default: %d)\n", DEFAULT_SPEED);
  printf("  -g, --gpio <pin>      GPIO interrupt pin number (default: %d)\n", DEFAULT_GPIO_PIN);
  printf("  -c, --chip <name>     GPIO chip name (default: %s)\n", DEFAULT_GPIO_CHIP);
  printf("  -h, --help            Show this help message\n");
}

// Dedicated display worker thread: logs telemetry to screen at a decoupled rate
static void display_worker() {
  uint64_t prev_count = 0;
  auto prev_time = std::chrono::steady_clock::now();

  while (g_running) {
    {
      std::unique_lock<std::mutex> cv_lock(g_display_cv_mtx);
      g_display_cv.wait_for(cv_lock,
                            std::chrono::milliseconds(g_config.display_interval_ms),
                            []{ return !g_running; });
    }

    if (!g_running) {
      break;
    }

    struct mpu9250_accel_data data;
    uint64_t count = 0;
    bool has_data = false;

    {
      std::lock_guard<std::mutex> lock(g_telemetry.mtx);
      data = g_telemetry.latest_accel;
      count = g_telemetry.sample_count;
      has_data = g_telemetry.has_data;
    }

    auto now = std::chrono::steady_clock::now();

    if (!has_data) {
      if (g_config.newline_mode) {
        printf("Waiting for interrupt events on GPIO %u...\n", g_config.gpio_pin);
      } else {
        printf("\rWaiting for interrupt events on GPIO %u...                ", g_config.gpio_pin);
      }
      fflush(stdout);
      continue;
    }

    std::chrono::duration<double> elapsed = now - prev_time;
    double rate = 0.0;
    if (elapsed.count() > 0.0) {
      rate = (double)(count - prev_count) / elapsed.count();
    }
    prev_count = count;
    prev_time = now;

    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_start_time).count();
    int mins = (total_ms / 60000);
    int secs = (total_ms / 1000) % 60;
    int ms = total_ms % 1000;

    if (g_config.newline_mode) {
      printf("[%02d:%02d.%03d] [Samples: %7llu | %5.1f Hz] Accel: X=%+6.2fg  Y=%+6.2fg  Z=%+6.2fg (raw: %6d, %6d, %6d)\n",
             mins, secs, ms,
             (unsigned long long)count,
             rate,
             data.ax, data.ay, data.az,
             data.raw_x, data.raw_y, data.raw_z);
    } else {
      printf("\r[%02d:%02d.%03d] [Samples: %7llu | %5.1f Hz] Accel: X=%+6.2fg  Y=%+6.2fg  Z=%+6.2fg (raw: %6d, %6d, %6d)   ",
             mins, secs, ms,
             (unsigned long long)count,
             rate,
             data.ax, data.ay, data.az,
             data.raw_x, data.raw_y, data.raw_z);
    }
    fflush(stdout);
  }
}

int main(int argc, char *argv[]) {
  // Default newline mode: true if stdout is redirected (pipe/file), false for interactive TTY
  g_config.newline_mode = !isatty(STDOUT_FILENO);

  static struct option long_options[] = {
    {"interval", required_argument, nullptr, 'i'},
    {"newline",  no_argument,       nullptr, 'n'},
    {"speed",    required_argument, nullptr, 's'},
    {"gpio",     required_argument, nullptr, 'g'},
    {"chip",     required_argument, nullptr, 'c'},
    {"help",     no_argument,       nullptr, 'h'},
    {nullptr,    0,                 nullptr, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "i:ns:g:c:h", long_options, nullptr)) != -1) {
    switch (opt) {
      case 'i':
        g_config.display_interval_ms = atoi(optarg);
        if (g_config.display_interval_ms < 10)
          g_config.display_interval_ms = 10;
        break;
      case 'n':
        g_config.newline_mode = true;
        break;
      case 's':
        speed = atoi(optarg);
        break;
      case 'g':
        g_config.gpio_pin = (unsigned int)atoi(optarg);
        break;
      case 'c':
        g_config.chip_name = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
      default:
        print_usage(argv[0]);
        return 1;
    }
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  int fd = spiOpen(0, speed, 0);
  if (fd < 0) {
    perror("spiOpen failed");
    return 1;
  }

  // Boot up sequence
  if (send_whoami(fd) != 1) {
    fprintf(stderr, "who am i failed\n");
    spiClose(fd);
    return 1;
  }

  if (powerup_seq(fd) != 1) {
    fprintf(stderr, "powerup sequence failed\r\n");
    spiClose(fd);
    return 1;
  }
  printf("MPU9250 initialized and powerup sequence done.\r\n");

  // Clear any initial pending interrupt
  read_accel(fd);

  // Initialize GPIO interrupt with libgpiod
  struct gpio_int_handle gpio_handle;
  if (gpio_interrupt_init(&gpio_handle, g_config.chip_name, g_config.gpio_pin, "mpu9250_drdy") < 0) {
    fprintf(stderr, "Failed to initialize GPIO interrupt on chip '%s' pin %u\n",
            g_config.chip_name, g_config.gpio_pin);
    spiClose(fd);
    return 1;
  }

  printf("Interrupt-driven sampling started on GPIO %u (%s)...\n",
         g_config.gpio_pin, g_config.chip_name);
  printf("Display refresh rate: every %d ms. Press Ctrl+C to stop.\n\n",
         g_config.display_interval_ms);

  struct mpu9250_accel_data accel_data;
  uint64_t sample_count = 0;

  g_start_time = std::chrono::steady_clock::now();

  // Start decoupled display worker thread
  std::thread display_th(display_worker);

  // Interrupt-driven acquisition loop:
  // Solely handles GPIO event wait + SPI read + fast telemetry snapshot.
  // Zero console or file I/O to avoid latency, jitter, or dropped samples.
  while (g_running) {
    int ret = gpio_interrupt_wait(&gpio_handle, 1000);
    if (ret > 0) {
      if (read_accel(fd, &accel_data)) {
        sample_count++;
        {
          std::lock_guard<std::mutex> lock(g_telemetry.mtx);
          g_telemetry.latest_accel = accel_data;
          g_telemetry.sample_count = sample_count;
          g_telemetry.has_data = true;
        }
      }
    }
  }

  auto end_time = std::chrono::steady_clock::now();

  // Wake up and join display thread
  g_display_cv.notify_all();
  if (display_th.joinable()) {
    display_th.join();
  }

  // Acquisition performance summary
  std::chrono::duration<double> total_duration = end_time - g_start_time;
  double avg_rate = total_duration.count() > 0.0 ? (double)sample_count / total_duration.count() : 0.0;

  printf("\n\n--- Acquisition Summary ---\n");
  printf("Total samples acquired : %llu\n", (unsigned long long)sample_count);
  printf("Total duration         : %.2f seconds\n", total_duration.count());
  printf("Average sampling rate  : %.2f Hz\n", avg_rate);

  gpio_interrupt_close(&gpio_handle);
  spiClose(fd);

  return 0;
}
