#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include "distance_sensor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(light, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *const tof_dev = DEVICE_DT_GET_ONE(st_vl53l0x);
static struct sensor_value prox;

int distance_sensor_init(void)
{	
	if (!device_is_ready(tof_dev)) 
	{
		printk("%s: device not ready.\n", tof_dev->name);
		return 0;
	}
	return 1;
}


uint16_t get_distance(void)
{
	uint16_t distance;
	int err1, err2;

	err1 = sensor_sample_fetch(tof_dev);

	err2 = sensor_channel_get(tof_dev, SENSOR_CHAN_DISTANCE, &prox);
	distance = prox.val1 * 100 + prox.val2/10000;

	//LOG_INF("Err1=%d Err2=%d Dis=%d", err1, err2, distance);

	return distance;
}


