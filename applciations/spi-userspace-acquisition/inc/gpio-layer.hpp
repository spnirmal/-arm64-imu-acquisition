#ifndef GPIO_LAYER_HPP
#define GPIO_LAYER_HPP

#include <gpiod.h>

struct gpio_int_handle {
  struct gpiod_chip *chip;
  struct gpiod_line *line;
};

// Initializes GPIO line for rising-edge event detection using libgpiod.
// chip_name: e.g. "gpiochip0", "/dev/gpiochip0", or nullptr (defaults to chip 0)
// line_num: GPIO line offset (e.g. 17 for BCM GPIO 17 / Pin 11)
// consumer: consumer name label (e.g. "mpu9250_drdy")
// Returns 0 on success, negative value on failure.
int gpio_interrupt_init(struct gpio_int_handle *handle,
                        const char *chip_name,
                        unsigned int line_num,
                        const char *consumer);

// Waits for an interrupt event on the configured line.
// timeout_ms: wait timeout in milliseconds (-1 for blocking indefinitely)
// Returns 1 if event occurred, 0 on timeout, -1 on error/interrupted.
int gpio_interrupt_wait(struct gpio_int_handle *handle, int timeout_ms);

// Closes and releases the GPIO line and chip resources.
void gpio_interrupt_close(struct gpio_int_handle *handle);

#endif // GPIO_LAYER_HPP
