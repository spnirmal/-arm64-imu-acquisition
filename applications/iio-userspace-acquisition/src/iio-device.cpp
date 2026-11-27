#include "iio-device.hpp"

#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

iio_device::iio_device(const std::string &preferred_name)
    : dev_name(preferred_name), dev_fd(-1), dev_num(0), packet_size(16), scale_g(1.0f / 16384.0f) {}

iio_device::~iio_device() {
  close_device();
}

#include <errno.h>
#include <utility>
#include <vector>

int iio_device::write_sysfs_int(const std::string &path, int val) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d\n", val);
  int fd = open(path.c_str(), O_WRONLY);
  if (fd < 0) {
    fprintf(stderr, "Error opening %s for write: %s (errno=%d)\n", path.c_str(), strerror(errno), errno);
    return -1;
  }
  ssize_t ret = write(fd, buf, strlen(buf));
  if (ret <= 0) {
    fprintf(stderr, "Error writing '%s' to %s: %s (errno=%d)\n", buf, path.c_str(), strerror(errno), errno);
  }
  close(fd);
  return (ret > 0) ? 0 : -1;
}

int iio_device::write_sysfs_string(const std::string &path, const std::string &val) {
  std::string to_write = val;
  if (to_write.empty() || to_write.back() != '\n') {
    to_write.push_back('\n');
  }
  int fd = open(path.c_str(), O_WRONLY);
  if (fd < 0) {
    fprintf(stderr, "Error opening %s for write: %s (errno=%d)\n", path.c_str(), strerror(errno), errno);
    return -1;
  }
  ssize_t ret = write(fd, to_write.c_str(), to_write.length());
  if (ret <= 0) {
    fprintf(stderr, "Error writing '%s' to %s: %s (errno=%d)\n", val.c_str(), path.c_str(), strerror(errno), errno);
  }
  close(fd);
  return (ret > 0) ? 0 : -1;
}

int iio_device::read_sysfs_string(const std::string &path, std::string &val) {
  char buf[128];
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return -1;
  ssize_t len = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (len <= 0) return -1;
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
    len--;
  }
  buf[len] = '\0';
  val = std::string(buf, len);
  return 0;
}

int iio_device::read_sysfs_float(const std::string &path, float &val) {
  char buf[64];
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return -1;
  ssize_t len = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (len <= 0) return -1;
  buf[len] = '\0';
  val = strtof(buf, nullptr);
  return 0;
}

bool iio_device::find_device() {
  const char *base = "/sys/bus/iio/devices";
  DIR *dir = opendir(base);
  if (!dir) {
    fprintf(stderr, "Cannot open %s: %s (errno=%d)\n", base, strerror(errno), errno);
    fprintf(stderr, "Is IIO subsystem enabled in the kernel?\n");
    return false;
  }

  struct dirent *ent;
  bool found = false;
  std::vector<std::pair<std::string, std::string>> all_devices;

  while ((ent = readdir(dir)) != nullptr) {
    if (strncmp(ent->d_name, "iio:device", 10) == 0) {
      std::string path = std::string(base) + "/" + ent->d_name;
      std::string name_path = path + "/name";
      std::string name_str;
      if (read_sysfs_string(name_path, name_str) == 0) {
        all_devices.push_back({ent->d_name, name_str});
      } else {
        all_devices.push_back({ent->d_name, ""});
      }
    }
  }
  closedir(dir);

  if (all_devices.empty()) {
    fprintf(stderr, "No IIO devices found in %s.\n", base);
    fprintf(stderr, "The sensor driver has not probed. Please verify:\n");
    fprintf(stderr, "  1. SPI master driver is loaded: run 'modprobe spi-bcm2835'\n");
    fprintf(stderr, "  2. Check kernel log: run 'dmesg | grep -i -E \"spi|mpu|iio\"'\n");
    return false;
  }

  // 1. Try to match preferred name, mpu, or spi0.0
  for (const auto &d : all_devices) {
    const std::string &node_name = d.first;
    const std::string &dev_label = d.second;

    bool match = (!dev_name.empty() && dev_label.find(dev_name) != std::string::npos) ||
                 (dev_label.find("mpu") != std::string::npos) ||
                 (dev_label.find("spi0.0") != std::string::npos);

    // Also check if scan_elements/in_accel_x_en exists
    std::string test_path = std::string(base) + "/" + node_name + "/scan_elements/in_accel_x_en";
    if (access(test_path.c_str(), F_OK) == 0) {
      match = true;
    }

    if (match) {
      sysfs_path = std::string(base) + "/" + node_name;
      dev_num = atoi(node_name.c_str() + 10);
      dev_node = "/dev/" + node_name;
      printf("Found matching IIO device: %s ('%s') at %s\n",
             node_name.c_str(), dev_label.c_str(), dev_node.c_str());
      found = true;
      break;
    }
  }

  // 2. If no explicit match but only 1 IIO device exists, use it
  if (!found && all_devices.size() == 1) {
    const auto &d = all_devices[0];
    sysfs_path = std::string(base) + "/" + d.first;
    dev_num = atoi(d.first.c_str() + 10);
    dev_node = "/dev/" + d.first;
    printf("Using single available IIO device: %s ('%s') at %s\n",
           d.first.c_str(), d.second.c_str(), dev_node.c_str());
    found = true;
  }

  if (!found) {
    fprintf(stderr, "Found %zu IIO device(s), but none matched MPU sensor.\n", all_devices.size());
    for (const auto &d : all_devices) {
      fprintf(stderr, "  - %s: name='%s'\n", d.first.c_str(), d.second.c_str());
    }
    return false;
  }

  return true;
}

bool iio_device::configure_trigger() {
  // First, check if current_trigger is already set
  std::string current_trig;
  if (read_sysfs_string(sysfs_path + "/trigger/current_trigger", current_trig) == 0 &&
      !current_trig.empty()) {
    printf("IIO current_trigger is already active: '%s'\n", current_trig.c_str());
    return true;
  }

  const char *base = "/sys/bus/iio/devices";
  DIR *dir = opendir(base);
  if (!dir) return false;

  struct dirent *ent;
  std::string trigger_name;
  std::vector<std::string> available_triggers;

  char dev_suffix[32];
  snprintf(dev_suffix, sizeof(dev_suffix), "-dev%d", dev_num);

  while ((ent = readdir(dir)) != nullptr) {
    if (strncmp(ent->d_name, "trigger", 7) == 0) {
      std::string name_path = std::string(base) + "/" + ent->d_name + "/name";
      std::string tname;
      if (read_sysfs_string(name_path, tname) == 0 && !tname.empty()) {
        available_triggers.push_back(tname);
        if (tname.find(dev_suffix) != std::string::npos ||
            tname.find("spi0.0") != std::string::npos ||
            tname.find("mpu") != std::string::npos) {
          trigger_name = tname;
        }
      }
    }
  }
  closedir(dir);

  if (trigger_name.empty() && !available_triggers.empty()) {
    // Fall back to first available trigger
    trigger_name = available_triggers[0];
  }

  if (!trigger_name.empty()) {
    printf("Binding trigger '%s' to %s/trigger/current_trigger\n", trigger_name.c_str(), sysfs_path.c_str());
    if (write_sysfs_string(sysfs_path + "/trigger/current_trigger", trigger_name) != 0) {
      fprintf(stderr, "Failed to bind trigger '%s' to %s/trigger/current_trigger\n",
              trigger_name.c_str(), sysfs_path.c_str());
      return false;
    }
  } else {
    fprintf(stderr, "Warning: No IIO triggers found under %s\n", base);
  }

  return true;
}

bool iio_device::configure_channels() {
  // Ensure buffer is disabled before modifying channels
  write_sysfs_int(sysfs_path + "/buffer/enable", 0);

  // Enable 3-axis accelerometer channels
  if (write_sysfs_int(sysfs_path + "/scan_elements/in_accel_x_en", 1) != 0) {
    fprintf(stderr, "Failed to enable in_accel_x_en\n");
  }
  if (write_sysfs_int(sysfs_path + "/scan_elements/in_accel_y_en", 1) != 0) {
    fprintf(stderr, "Failed to enable in_accel_y_en\n");
  }
  if (write_sysfs_int(sysfs_path + "/scan_elements/in_accel_z_en", 1) != 0) {
    fprintf(stderr, "Failed to enable in_accel_z_en\n");
  }

  // Enable hardware timestamp channel
  if (write_sysfs_int(sysfs_path + "/scan_elements/in_timestamp_en", 1) != 0) {
    fprintf(stderr, "Failed to enable in_timestamp_en\n");
  }

  // Set buffer length (e.g. 2048 entries)
  if (write_sysfs_int(sysfs_path + "/buffer/length", 2048) != 0) {
    fprintf(stderr, "Failed to set buffer length\n");
  }

  // Attempt to read scale factor if available (m/s^2 per LSB)
  float raw_scale_m_s2 = 0.0f;
  if (read_sysfs_float(sysfs_path + "/in_accel_scale", raw_scale_m_s2) == 0 && raw_scale_m_s2 > 0.0f) {
    // 1 g = 9.80665 m/s^2 -> scale in g = raw_scale / 9.80665
    scale_g = raw_scale_m_s2 / 9.80665f;
    printf("Read sysfs accel scale: %.6f m/s^2 (%.6f g/LSB)\n", raw_scale_m_s2, scale_g);
  } else {
    // Default ±2g sensitivity is 16384 LSB/g
    scale_g = 1.0f / 16384.0f;
    printf("Using default ±2g accel scale: 16384 LSB/g\n");
  }

  return true;
}

bool iio_device::enable_buffer(bool enable) {
  int ret = write_sysfs_int(sysfs_path + "/buffer/enable", enable ? 1 : 0);
  if (ret != 0) {
    fprintf(stderr, "write_sysfs_int %s/buffer/enable %d failed: %s (errno=%d)\n",
            sysfs_path.c_str(), enable ? 1 : 0, strerror(errno), errno);
    return false;
  }
  return true;
}

bool iio_device::init() {
  if (!find_device()) {
    fprintf(stderr, "Failed to find IIO device\n");
    return false;
  }

  configure_trigger();
  configure_channels();

  if (!enable_buffer(true)) {
    fprintf(stderr, "Failed to enable IIO buffer at %s/buffer/enable\n", sysfs_path.c_str());
    return false;
  }

  dev_fd = open(dev_node.c_str(), O_RDONLY | O_NONBLOCK);
  if (dev_fd < 0) {
    perror("open IIO char device");
    enable_buffer(false);
    return false;
  }

  printf("IIO triggered buffer enabled successfully. Streaming from %s\n", dev_node.c_str());
  return true;
}

void iio_device::close_device() {
  if (dev_fd >= 0) {
    close(dev_fd);
    dev_fd = -1;
  }
  if (!sysfs_path.empty()) {
    enable_buffer(false);
  }
}

bool iio_device::read_sample(IioSample &sample, int timeout_ms) {
  if (dev_fd < 0) return false;

  struct pollfd pfd = { dev_fd, POLLIN, 0 };
  int poll_ret = poll(&pfd, 1, timeout_ms);
  if (poll_ret <= 0) {
    return false;
  }

  // IIO scan packet layout for 3-axis accel + timestamp:
  // int16_t accel_x (be)  [bytes 0-1]
  // int16_t accel_y (be)  [bytes 2-3]
  // int16_t accel_z (be)  [bytes 4-5]
  // padding               [bytes 6-7]
  // int64_t timestamp_ns  [bytes 8-15]
  uint8_t raw_packet[16];
  ssize_t bytes_read = read(dev_fd, raw_packet, sizeof(raw_packet));

  if (clock_gettime(CLOCK_MONOTONIC, &sample.post_read_ts) != 0) {
    sample.post_read_ts = {};
  }

  if (bytes_read != (ssize_t)sizeof(raw_packet)) {
    return false;
  }

  int16_t raw_x = (int16_t)(((uint16_t)raw_packet[0] << 8) | (uint16_t)raw_packet[1]);
  int16_t raw_y = (int16_t)(((uint16_t)raw_packet[2] << 8) | (uint16_t)raw_packet[3]);
  int16_t raw_z = (int16_t)(((uint16_t)raw_packet[4] << 8) | (uint16_t)raw_packet[5]);

  sample.ax = (float)raw_x * scale_g;
  sample.ay = (float)raw_y * scale_g;
  sample.az = (float)raw_z * scale_g;

  int64_t kernel_ts_ns = 0;
  memcpy(&kernel_ts_ns, &raw_packet[8], sizeof(int64_t));

  sample.kernel_ts.tv_sec = (time_t)(kernel_ts_ns / 1000000000LL);
  sample.kernel_ts.tv_nsec = (long)(kernel_ts_ns % 1000000000LL);

  return true;
}
