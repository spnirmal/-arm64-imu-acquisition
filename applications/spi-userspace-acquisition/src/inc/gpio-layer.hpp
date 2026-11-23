#ifndef GPIO_LAYER_HPP
#define GPIO_LAYER_HPP

#include <gpiod.h>
#include <time.h>

#define ACCEL_INT_GPIO 17

class gpio {
private:
  int pin_number;
  int event_fd;
  struct gpiod_chip *chip;
  struct gpiod_line *line;

public:
  gpio(int pin_number);
  ~gpio();

  int configure();
  int wait_event(int timeout_ms = -1);
  int wait_event(struct timespec *event_ts, int timeout_ms = -1);
  int wait_event(struct gpiod_line_event *event_out, int timeout_ms = -1);

  int get_event_fd() const { return event_fd; }

  struct gpiod_chip *get_chip() const { return chip; }
  struct gpiod_line *get_line() const { return line; }
  int get_pin() const { return pin_number; }
  bool is_valid() const { return chip != nullptr && line != nullptr; }
};

#endif
