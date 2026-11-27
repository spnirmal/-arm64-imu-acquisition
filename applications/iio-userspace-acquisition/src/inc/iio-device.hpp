#ifndef IIO_DEVICE_HPP
#define IIO_DEVICE_HPP

#include <stdint.h>
#include <string>
#include <time.h>

struct IioSample {
  float ax;
  float ay;
  float az;
  struct timespec kernel_ts;
  struct timespec post_read_ts;
};

class iio_device {
private:
  std::string dev_name;
  std::string sysfs_path;
  std::string dev_node;
  int dev_fd;
  int dev_num;
  size_t packet_size;
  float scale_g;

  bool find_device();
  bool configure_trigger();
  bool configure_channels();
  bool enable_buffer(bool enable);
  int write_sysfs_int(const std::string &path, int val);
  int write_sysfs_string(const std::string &path, const std::string &val);
  int read_sysfs_string(const std::string &path, std::string &val);
  int read_sysfs_float(const std::string &path, float &val);

public:
  iio_device(const std::string &preferred_name = "mpu9250");
  ~iio_device();

  bool init();
  void close_device();

  // Read single sample from triggered buffer
  bool read_sample(IioSample &sample, int timeout_ms = 1000);

  const std::string &get_sysfs_path() const { return sysfs_path; }
  const std::string &get_dev_node() const { return dev_node; }
};

#endif // IIO_DEVICE_HPP
