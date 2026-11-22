#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "mpu9250.hpp"
#include "spi-device.hpp"

#define LOOPS 10000
#define SPEED 100000
#define BYTES 3

int bytes = BYTES;
int speed = SPEED;
int loops = LOOPS;

int main() {

  mpu9250 accelerometer(speed);

  while (1) {
    accelerometer.read_accel();
    sleep(1);
  }
}
