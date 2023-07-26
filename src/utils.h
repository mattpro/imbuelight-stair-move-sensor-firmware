#ifndef __UTILS_H
#define __UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

extern const struct device *dev_led;

void led_init(void);
void led_start(void);
void set_led(int state);


void out_init(void);
void set_out(int state);

#endif