#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include "distance_sensor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(light, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *const tof_dev = DEVICE_DT_GET_ONE(st_vl53l0x);
static struct sensor_value prox;

void distance_sensor_init(void)
{	
	if (!device_is_ready(tof_dev)) 
	{
		printk("%s: device not ready.\n", tof_dev->name);
		return 0;
	}
}


int get_distance(void)
{
	int distance;

	sensor_sample_fetch(tof_dev);
	sensor_channel_get(tof_dev, SENSOR_CHAN_DISTANCE, &prox);
	distance = prox.val1/8 * 100 + prox.val2/10000;

	return distance;
}


