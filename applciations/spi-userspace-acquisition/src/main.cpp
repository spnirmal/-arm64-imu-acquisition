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

#define LOOPS 10000
#define SPEED 100000
#define BYTES 3

int send_whoami(int fd);
int mpu9250_write_reg(int fd, uint8_t reg, uint8_t value);
int powerup_seq(int fd);
int read_accel(int fd);

#define MPU9250_PWR_MGMT_1 0x6B
#define MPU9250_PWR_MGMT_2 0x6C
#define MPU9250_CONFIG 0x1A
#define MPU9250_SMPLRT_DIV 0x19
#define MPU9250_GYRO_CONFIG 0x1B
#define MPU9250_ACCEL_CONFIG 0x1C
#define MPU9250_ACCEL_CONFIG2 0x1D
#define MPU9250_ACCEL_XOUT_H 0x3B

double time_time(void) {
  struct timeval tv;
  double t;

  gettimeofday(&tv, 0);

  t = (double)tv.tv_sec + ((double)tv.tv_usec / 1E6);

  return t;
}

int spiOpen(unsigned spiChan, unsigned spiBaud, unsigned spiFlags) {
  int fd;
  char spiMode;
  char spiBits = 8;
  char dev[32];

  spiMode = spiFlags & 3;
  spiBits = 8;

  sprintf(dev, "/dev/spidev0.%d", spiChan);

  if ((fd = open(dev, O_RDWR)) < 0) {
    return -1;
  }

  if (ioctl(fd, SPI_IOC_WR_MODE, &spiMode) < 0) {
    close(fd);
    return -2;
  }

  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits) < 0) {
    close(fd);
    return -3;
  }

  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiBaud) < 0) {
    close(fd);
    return -4;
  }

  return fd;
}

int spiClose(int fd) { return close(fd); }

int spiRead(int fd, unsigned speed, char *buf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (unsigned)NULL;
  spi.rx_buf = (__u64)buf;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

int spiWrite(int fd, unsigned speed, char *buf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (__u64)buf;
  spi.rx_buf = (__u64)NULL;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

int spiXfer(int fd, unsigned speed, char *txBuf, char *rxBuf, unsigned count) {
  int err;
  struct spi_ioc_transfer spi;

  memset(&spi, 0, sizeof(spi));

  spi.tx_buf = (unsigned long)txBuf;
  spi.rx_buf = (unsigned long)rxBuf;
  spi.len = count;
  spi.speed_hz = speed;
  spi.delay_usecs = 0;
  spi.bits_per_word = 8;
  spi.cs_change = 0;

  err = ioctl(fd, SPI_IOC_MESSAGE(1), &spi);

  return err;
}

#define MAX_SPI_BUFSIZ 8192

char RXBuf[MAX_SPI_BUFSIZ];
char TXBuf[MAX_SPI_BUFSIZ];

int bytes = BYTES;
int speed = SPEED;
int loops = LOOPS;

int main(int argc, char *argv[]) {
  int i, fd;
  double start, diff, sps;

  if (argc > 1)
    bytes = atoi(argv[1]);
  else
    printf("./spi-driver-speed [bytes [bps [loops] ] ]\n\n");

  if ((bytes < 1) || (bytes > MAX_SPI_BUFSIZ))
    bytes = BYTES;

  if (argc > 2)
    speed = atoi(argv[2]);
  if ((speed < 32000) || (speed > 250000000))
    speed = SPEED;

  if (argc > 3)
    loops = atoi(argv[3]);
  if ((loops < 1) || (loops > 10000000))
    loops = LOOPS;

  fd = spiOpen(0, speed, 0);

  if (fd < 0)
    return 1;

  if (send_whoami(fd) != 1) {
    printf("who am i failed\n");
  }

  powerup_seq(fd);
  printf("powerup sequence done\r\n");

  while (1) {
    read_accel(fd);
    sleep(1);
  }

  close(fd);
}

int mpu9250_write_reg(int fd, uint8_t reg, uint8_t value) {
  TXBuf[0] = reg & 0x7F; // Write operation
  TXBuf[1] = value;

  int ret = spiXfer(fd, speed, TXBuf, RXBuf, 2);

  if (ret != 2) {
    perror("SPI register write failed");
    return 0;
  }

  return 1;
}

int send_whoami(int fd) {

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

int powerup_seq(int fd) {
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

  // 5. Sample rate divider
  if (!mpu9250_write_reg(fd, MPU9250_SMPLRT_DIV, 0x04))
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

  usleep(10000);

  return 1;
}

int read_accel(int fd) {
  memset(TXBuf, 0, 7);
  TXBuf[0] = (char)(MPU9250_ACCEL_XOUT_H | 0x80); // Read operation

  int ret = spiXfer(fd, speed, TXBuf, RXBuf, 7);
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

  printf("Accel: X=%.2f g, Y=%.2f g, Z=%.2f g\n", ax, ay, az);
  fflush(stdout);

  return 1;
}
