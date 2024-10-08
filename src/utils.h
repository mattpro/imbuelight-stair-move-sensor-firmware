#ifndef __UTILS_H
#define __UTILS_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

extern const struct device *dev_led;

void LED_init(void);
void LED_start(void);
void LED_set(int state);

void LED_connected_signalization(void);
void LED_disconnect_signalization(void);
void LED_save_signalization(void);


void OUT_init(void);
void OUT_set(int state);

#endif