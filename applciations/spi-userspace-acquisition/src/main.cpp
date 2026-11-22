#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "gpio-layer.hpp"
#include "mpu9250.hpp"
#include "spi-device.hpp"
#include "version.hpp"

#define LOOPS 10000
#define SPEED 100000
#define BYTES 3

int bytes = BYTES;
int speed = SPEED;
int loops = LOOPS;

int main(int argc, char *argv[]) {
  // Print version string as the first thing upon running
  printf("%s version %s\n", APP_NAME, APP_VERSION);
  fflush(stdout);

  // If invoked with -v / --version, exit immediately after printing version
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      return 0;
    }
  }

  gpio accel_int_gpio(ACCEL_INT_GPIO);
  accel_int_gpio.configure();

  mpu9250 accelerometer(speed);

  while (1) {
    if (accel_int_gpio.wait_event() > 0) {
      accelerometer.read_accel();
    }
  }
}
