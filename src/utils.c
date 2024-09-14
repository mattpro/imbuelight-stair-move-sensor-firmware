#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "utils.h"
#include "settings.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(utils, CONFIG_LOG_DEFAULT_LEVEL);


#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define OUT_NODE DT_ALIAS(out0)
static const struct gpio_dt_spec out = GPIO_DT_SPEC_GET(OUT_NODE, gpios);


void LED_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) 
	{
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}

void LED_start(void)
{
    for(int i = 0 ; i < 20 ; i ++ )
    {
        gpio_pin_toggle_dt(&led);
		k_msleep(25);
    }

	gpio_pin_set_dt(&led, 0);
}


void LED_set(int state)
{
    gpio_pin_set_dt(&led, state);
}


void OUT_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&out)) 
	{
		return;
	}

	ret = gpio_pin_configure_dt(&out, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) 
	{
		return;
	}
	gpio_pin_set_dt(&out, 0);
}


void OUT_set(int state)
{
    gpio_pin_set_dt(&out, state);
}


void LED_connected_signalization(void)
{
	for (int i = 0 ; i < 5 ; i ++ )
	{
		LED_set(true);
		k_msleep(50);
		LED_set(false);
		k_msleep(50);
	}
	LED_set(led_state);
}


void LED_disconnect_signalization(void)
{
	for (int i = 0 ; i < 3 ; i ++ )
	{
		LED_set(true);
		k_msleep(50);
		LED_set(false);
		k_msleep(50);
	}
	LED_set(led_state);
}

		
void LED_save_signalization(void)
{
	for (int i = 0 ; i < 5 ; i ++ )
	{
		LED_set(true);
		k_msleep(50);
		LED_set(false);
		k_msleep(50);
	}
	LED_set(led_state);
}

		