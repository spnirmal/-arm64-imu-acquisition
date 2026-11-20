#include "gpio-layer.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int gpio_interrupt_init(struct gpio_int_handle *handle,
                        const char *chip_name,
                        unsigned int line_num,
                        const char *consumer) {
  if (!handle)
    return -1;

  handle->chip = nullptr;
  handle->line = nullptr;

  if (chip_name && chip_name[0] != '\0') {
    handle->chip = gpiod_chip_open_lookup(chip_name);
    if (!handle->chip)
      handle->chip = gpiod_chip_open(chip_name);
    if (!handle->chip)
      handle->chip = gpiod_chip_open_by_name(chip_name);
  }

  // Fallback defaults for Raspberry Pi
  if (!handle->chip)
    handle->chip = gpiod_chip_open_by_number(0);
  if (!handle->chip)
    handle->chip = gpiod_chip_open("/dev/gpiochip0");
  if (!handle->chip)
    handle->chip = gpiod_chip_open_by_name("gpiochip0");

  if (!handle->chip) {
    perror("libgpiod: failed to open gpiochip");
    return -1;
  }

  handle->line = gpiod_chip_get_line(handle->chip, line_num);
  if (!handle->line) {
    perror("libgpiod: failed to get line");
    gpiod_chip_close(handle->chip);
    handle->chip = nullptr;
    return -2;
  }

  if (gpiod_line_request_rising_edge_events(handle->line, consumer) < 0) {
    perror("libgpiod: failed to request rising edge events");
    gpiod_chip_close(handle->chip);
    handle->chip = nullptr;
    handle->line = nullptr;
    return -3;
  }

  return 0;
}

int gpio_interrupt_wait(struct gpio_int_handle *handle, int timeout_ms) {
  if (!handle || !handle->line)
    return -1;

  struct timespec ts;
  struct timespec *pts = nullptr;

  if (timeout_ms >= 0) {
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
    pts = &ts;
  }

  int ret = gpiod_line_event_wait(handle->line, pts);
  if (ret > 0) {
    struct gpiod_line_event event;
    if (gpiod_line_event_read(handle->line, &event) == 0) {
      return 1;
    }
    return -1;
  } else if (ret == 0) {
    return 0; // Timeout
  } else {
    return -1; // Interrupted or error
  }
}

void gpio_interrupt_close(struct gpio_int_handle *handle) {
  if (!handle)
    return;

  if (handle->line) {
    gpiod_line_release(handle->line);
    handle->line = nullptr;
  }

  if (handle->chip) {
    gpiod_chip_close(handle->chip);
    handle->chip = nullptr;
  }
}
