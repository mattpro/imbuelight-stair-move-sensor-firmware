#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include "utils.h"
#include "adc.h"
#include "distance_sensor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);


int distance = 0;
int light = 0;


void main(void)
{
	LOG_INF("Imbue light move controller ver 1.0 start");

	led_init();
	out_init();
	adc_init();
	distance_sensor_init();
	led_start();

	while (1) 
	{
		distance = get_distance();
		light = get_light_intensity();

		LOG_INF("distance: %4d cm light: %4d", distance, light); 

		//LOG_INF("x: %d y: %d", prox.val1, prox.val2/10000);
		if ( distance < 80 )
		{
			set_led(1);
			set_out(1);
		}
		else
		{
			set_led(0);
			set_out(0);
		}

		k_msleep(50);
	}
}
