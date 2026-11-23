#include "gpio-layer.hpp"
#include <cstdio>
#include <gpiod.h>

gpio::gpio(int pin_number)
    : pin_number(pin_number), event_fd(-1), chip(nullptr), line(nullptr) {
  this->chip = gpiod_chip_open("/dev/gpiochip0");
  if (!this->chip) {
    perror("gpiod_chip_open");
    return;
  }

  this->line = gpiod_chip_get_line(this->chip, pin_number);
  if (!this->line) {
    perror("gpiod_chip_get_line");
    gpiod_chip_close(this->chip);
    this->chip = nullptr;
    return;
  }
}

gpio::~gpio() {
  if (line) {
    if (gpiod_line_is_requested(line)) {
      gpiod_line_release(line);
    }
    line = nullptr;
  }

  if (chip) {
    gpiod_chip_close(chip);
    chip = nullptr;
  }
  event_fd = -1;
}

int gpio::configure() {
  if (!this->line) {
    fprintf(stderr, "gpio::configure: line handle is not valid\n");
    return -1;
  }

  if (gpiod_line_is_requested(this->line)) {
    gpiod_line_release(this->line);
  }

  int ret = gpiod_line_request_rising_edge_events(this->line, "mpu9250_int");
  if (ret < 0) {
    perror("gpiod_line_request_rising_edge_events");
    return -1;
  }

  this->event_fd = gpiod_line_event_get_fd(this->line);
  if (this->event_fd < 0) {
    perror("gpiod_line_event_get_fd");
    return -1;
  }

  return 0;
}

int gpio::wait_event(int timeout_ms) {
  return wait_event(static_cast<struct timespec *>(nullptr), timeout_ms);
}

int gpio::wait_event(struct timespec *event_ts, int timeout_ms) {
  if (!this->line) {
    return -1;
  }

  struct timespec ts;
  struct timespec *ts_ptr = nullptr;
  if (timeout_ms >= 0) {
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
    ts_ptr = &ts;
  }

  int ret = gpiod_line_event_wait(this->line, ts_ptr);
  if (ret <= 0) {
    return ret; // 0 on timeout, -1 on error
  }

  struct gpiod_line_event event;
  if (gpiod_line_event_read(this->line, &event) < 0) {
    perror("gpiod_line_event_read");
    return -1;
  }

  if (event_ts) {
    *event_ts = event.ts;
  }

  return 1;
}

int gpio::wait_event(struct gpiod_line_event *event_out, int timeout_ms) {
  if (!this->line) {
    return -1;
  }

  struct timespec ts;
  struct timespec *ts_ptr = nullptr;
  if (timeout_ms >= 0) {
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
    ts_ptr = &ts;
  }

  int ret = gpiod_line_event_wait(this->line, ts_ptr);
  if (ret <= 0) {
    return ret; // 0 on timeout, -1 on error
  }

  struct gpiod_line_event event;
  if (gpiod_line_event_read(this->line, &event) < 0) {
    perror("gpiod_line_event_read");
    return -1;
  }

  if (event_out) {
    *event_out = event;
  }

  return 1;
}
