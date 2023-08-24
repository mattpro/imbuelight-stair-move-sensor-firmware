#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "utils.h"
#include "adc.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

#include <nrfx_saadc.h>

int distance = 0;
int light = 0;
struct sensor_value prox;



void main(void)
{
	LOG_INF("Imbue light move controller ver 1.0 start");

	led_init();
	out_init();
	adc_init();
	led_start();

	const struct device *const tof_dev = DEVICE_DT_GET_ONE(st_vl53l0x);
	if (!device_is_ready(tof_dev)) {
		printk("%s: device not ready.\n", tof_dev->name);
		return 0;
	}
	

	while (1) 
	{
		sensor_sample_fetch(tof_dev);
		sensor_channel_get(tof_dev, SENSOR_CHAN_DISTANCE, &prox);
		distance = prox.val1/8 * 100 + prox.val2/10000;
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
