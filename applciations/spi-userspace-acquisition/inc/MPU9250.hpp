#ifndef MPU9250_HPP
#define MPU9250_HPP

#include <stdint.h>

#define MPU9250_SMPLRT_DIV 0x19
#define MPU9250_CONFIG 0x1A
#define MPU9250_GYRO_CONFIG 0x1B
#define MPU9250_ACCEL_CONFIG 0x1C
#define MPU9250_ACCEL_CONFIG2 0x1D
#define MPU9250_INT_PIN_CFG 0x37
#define MPU9250_INT_ENABLE 0x38
#define MPU9250_INT_STATUS 0x3A
#define MPU9250_ACCEL_XOUT_H 0x3B
#define MPU9250_PWR_MGMT_1 0x6B
#define MPU9250_PWR_MGMT_2 0x6C
#define MPU9250_WHO_AM_I 0x75

struct mpu9250_accel_data {
  float ax;
  float ay;
  float az;
  int16_t raw_x;
  int16_t raw_y;
  int16_t raw_z;
};

int send_whoami(int fd);
int mpu9250_write_reg(int fd, uint8_t reg, uint8_t value);
int mpu9250_enable_interrupt(int fd);
int powerup_seq(int fd);
int read_accel(int fd, struct mpu9250_accel_data *data = nullptr);

#endif // MPU9250_HPP
