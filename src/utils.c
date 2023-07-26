#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "utils.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(utils, CONFIG_LOG_DEFAULT_LEVEL);


#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define OUT_NODE DT_ALIAS(out0)
static const struct gpio_dt_spec out = GPIO_DT_SPEC_GET(OUT_NODE, gpios);


void led_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) 
	{
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) 
	{
		return 0;
	}
}


void led_start(void)
{
    bool led_is_on;

    for(int i = 0 ; i < 40 ; i ++ )
    {
        gpio_pin_toggle_dt(&led);
		k_msleep(10);
    }
}


void set_led(int state)
{
    gpio_pin_set_dt(&led, state);
}


void out_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&out)) 
	{
		return 0;
	}

	ret = gpio_pin_configure_dt(&out, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) 
	{
		return 0;
	}
	gpio_pin_set_dt(&out, 0);
}


void set_out(int state)
{
    gpio_pin_set_dt(&out, state);
}
