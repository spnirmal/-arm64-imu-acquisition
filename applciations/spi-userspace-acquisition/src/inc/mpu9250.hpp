#ifndef MPU9250_HPP
#define MPU9250_HPP

#include <stdint.h>

#define MPU9250_PWR_MGMT_1 0x6B
#define MPU9250_PWR_MGMT_2 0x6C
#define MPU9250_CONFIG 0x1A
#define MPU9250_SMPLRT_DIV 0x19
#define MPU9250_GYRO_CONFIG 0x1B
#define MPU9250_ACCEL_CONFIG 0x1C
#define MPU9250_ACCEL_CONFIG2 0x1D
#define MPU9250_ACCEL_XOUT_H 0x3B

class mpu9250 {
private:
  int fd;
  int speed;

  int powerup_seq(int fd);

  int mpu9250_write_reg(int fd, uint8_t reg, uint8_t value);

public:
  mpu9250(int speed); // constructor

  ~mpu9250(); // destructor

  int read_accel();
};

#endif
