#include "mpu9250.hpp"
#include "spi-device.hpp"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int send_whoami(int fd, int speed);
int powerup_seq(int fd);
int mpu9250_write_reg(int fd, uint8_t reg, uint8_t value);

// constructor
mpu9250::mpu9250(int speed) {
  this->speed = speed;
  fd = spiOpen(0, speed, 0);

  if (fd < 0) {
    printf("fd not acquired\n");
  }

  if (send_whoami(fd, speed) != 1) {
    printf("read who am i failed\n");
  }

  if (powerup_seq(fd) != 1) {
    printf("power up sequence failed\n");
  }
}

mpu9250::~mpu9250() { close(fd); }

int send_whoami(int fd, int speed) {

  memset(&TXBuf[0], 0xF5, 1); // sending who ami i
  memset(&TXBuf[1], 0x00,
         1); // sending 0x00 to get the response from the mpu9250

  int count = 2;

  int ret = spiXfer(fd, speed, TXBuf, RXBuf, count);
  if (ret < 0) {
    perror("spi transfer failed\n");
    close(fd);
    return 0;
  }

  printf("who am i : 0x%02X\n", (unsigned char)RXBuf[1]);
  return 1;
}

int mpu9250::powerup_seq(int fd) {
  // 1. Reset device
  if (!mpu9250_write_reg(fd, MPU9250_PWR_MGMT_1, 0x80))
    return 0;

  usleep(100000); // Wait 100 ms

  // 2. Select PLL clock with X-axis gyro reference
  if (!mpu9250_write_reg(fd, MPU9250_PWR_MGMT_1, 0x01))
    return 0;

  usleep(10000);

  // 3. Enable accelerometer and gyroscope axes
  if (!mpu9250_write_reg(fd, MPU9250_PWR_MGMT_2, 0x00))
    return 0;

  // 4. Configure digital low-pass filter
  if (!mpu9250_write_reg(fd, MPU9250_CONFIG, 0x03))
    return 0;

  // 5. Sample rate divider: SMPLRT_DIV = 0 -> 1000 Hz / (1 + 0) = 1000 Hz
  if (!mpu9250_write_reg(fd, MPU9250_SMPLRT_DIV, 0x00))
    return 0;

  // 6. Gyroscope: ±250 degrees/sec
  if (!mpu9250_write_reg(fd, MPU9250_GYRO_CONFIG, 0x00))
    return 0;

  // 7. Accelerometer: ±2g
  if (!mpu9250_write_reg(fd, MPU9250_ACCEL_CONFIG, 0x00))
    return 0;

  // 8. Accelerometer low-pass filter
  if (!mpu9250_write_reg(fd, MPU9250_ACCEL_CONFIG2, 0x03))
    return 0;

  // 9. inteerupt enable
  if (!mpu9250_write_reg(fd, MPU9250_INT_ENABLE, 0x01))
    return 0;

  // Clear latched interrupt
  if (!mpu9250_read_reg(fd, MPU9250_INT_STATUS))
    return 0;

  usleep(10000);

  return 1;
}

int mpu9250::mpu9250_read_reg(int fd, uint8_t reg, uint8_t *value) {
  TXBuf[0] = reg | 0x80; // Read operation
  TXBuf[1] = 0x00;

  int ret = spiXfer(fd, this->speed, TXBuf, RXBuf, 2);

  if (ret != 2) {
    perror("SPI register read failed");
    return 0;
  }

  if (value) {
    *value = (uint8_t)RXBuf[1];
  }

  return 1;
}

int mpu9250::mpu9250_write_reg(int fd, uint8_t reg, uint8_t value) {
  TXBuf[0] = reg & 0x7F; // Write operation
  TXBuf[1] = value;

  int ret = spiXfer(fd, this->speed, TXBuf, RXBuf, 2);

  if (ret != 2) {
    perror("SPI register write failed");
    return 0;
  }

  return 1;
}

int mpu9250::read_accel() {
  return read_accel(nullptr, nullptr, nullptr, nullptr);
}

int mpu9250::read_accel(float *ax_out, float *ay_out, float *az_out, struct timespec *post_spi_ts) {
  memset(TXBuf, 0, 7);
  TXBuf[0] = (char)(MPU9250_ACCEL_XOUT_H | 0x80); // Read operation starting at 0x3B

  int ret = spiXfer(fd, speed, TXBuf, RXBuf, 7);
  if (post_spi_ts) {
    clock_gettime(CLOCK_MONOTONIC, post_spi_ts);
  }
  if (ret < 0) {
    perror("spi transfer failed\n");
    return 0;
  }

  int16_t raw_x = (int16_t)(((uint8_t)RXBuf[1] << 8) | (uint8_t)RXBuf[2]);
  int16_t raw_y = (int16_t)(((uint8_t)RXBuf[3] << 8) | (uint8_t)RXBuf[4]);
  int16_t raw_z = (int16_t)(((uint8_t)RXBuf[5] << 8) | (uint8_t)RXBuf[6]);

  // ±2g sensitivity is 16384 LSB/g
  float ax = (float)raw_x / 16384.0f;
  float ay = (float)raw_y / 16384.0f;
  float az = (float)raw_z / 16384.0f;

  if (ax_out) *ax_out = ax;
  if (ay_out) *ay_out = ay;
  if (az_out) *az_out = az;

  return 1;
}
