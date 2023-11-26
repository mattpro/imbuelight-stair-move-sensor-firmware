#include <zephyr/kernel.h>
#include <nrfx_saadc.h>
#include "settings.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(settings, CONFIG_LOG_DEFAULT_LEVEL);

struct settings_t settings;

static struct nvs_fs fs;

void SETTINGS_load_default(void)
{
	settings.threshold_distance 		= DEFAULT_SETTINGS_THRESHOLD_DISTANCE;
	settings.threshold_light_intensity 	= DEFAULT_SETTINGS_THRESHOLD_LIGHT_INTENSITY;
	settings.enable_distance 			= DEFAULT_SETTINGS_ENABLE_DISTANCE;
	settings.enable_light_intensity 	= DEFAULT_SETTINGS_ENABLE_LIGHT_INTENSITY;
	settings.enable_led_signalization 	= DEFAULT_SETTINGS_ENABLE_LED_SIGNALIZATION;
}


void SETTINGS_init(void)
{
	int rc = 0;
	struct flash_pages_info info;

	/* define the nvs file system by settings with:
	 *	sector_size equal to the pagesize,
	 *	3 sectors
	 *	starting at NVS_PARTITION_OFFSET
	 */
	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		printk("Flash device %s is not ready\n", fs.flash_device->name);
		return 0;
	}
	fs.offset = NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
		return 0;
	}
	fs.sector_size = info.size;
	fs.sector_count = 3U;

	rc = nvs_mount(&fs);
	if (rc) {
		printk("Flash Init failed\n");
		return 0;
	}
}


void SETTINGS_load(void)
{
	int rc = 0;

	rc = nvs_read(&fs, SETTINGS_DISTANCE_ID, &settings.threshold_distance, sizeof(settings.threshold_distance));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", SETTINGS_DISTANCE_ID, settings.threshold_distance);
	} 
	else   
	{/* item was not found, add it */
		settings.threshold_distance = DEFAULT_SETTINGS_THRESHOLD_DISTANCE;
		LOG_WRN("No settings found, set default %d cm", settings.threshold_distance);
		(void)nvs_write(&fs, SETTINGS_DISTANCE_ID, &settings.threshold_distance, sizeof(settings.threshold_distance));
	}

	rc = nvs_read(&fs, SETTINGS_LIGHT_ID, &settings.threshold_light_intensity, sizeof(settings.threshold_light_intensity));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", SETTINGS_LIGHT_ID, settings.threshold_light_intensity);
	} 
	else   
	{/* item was not found, add it */
		settings.threshold_light_intensity = DEFAULT_SETTINGS_THRESHOLD_LIGHT_INTENSITY;
		LOG_WRN("No settings found, set default %d", settings.threshold_light_intensity);
		(void)nvs_write(&fs, SETTINGS_LIGHT_ID, &settings.threshold_light_intensity, sizeof(settings.threshold_light_intensity));
	}

	rc = nvs_read(&fs, SETTINGS_DISTANCE_ENABLE_ID, &settings.enable_distance, sizeof(settings.enable_distance));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", SETTINGS_DISTANCE_ENABLE_ID, settings.enable_distance);
	} 
	else   
	{/* item was not found, add it */
		settings.enable_distance = DEFAULT_SETTINGS_ENABLE_DISTANCE;
		LOG_WRN("No settings found, set default %d", settings.enable_distance);
		(void)nvs_write(&fs, SETTINGS_DISTANCE_ENABLE_ID, &settings.enable_distance, sizeof(settings.enable_distance));
	}

	rc = nvs_read(&fs, SETTINGS_LIGHT_ENABLE_ID, &settings.enable_light_intensity, sizeof(settings.enable_light_intensity));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", SETTINGS_LIGHT_ENABLE_ID, settings.enable_light_intensity);
	} 
	else   
	{/* item was not found, add it */
		settings.enable_light_intensity = DEFAULT_SETTINGS_ENABLE_DISTANCE;
		LOG_WRN("No settings found, set default %d", settings.enable_light_intensity);
		(void)nvs_write(&fs, SETTINGS_LIGHT_ENABLE_ID, &settings.enable_light_intensity, sizeof(settings.enable_light_intensity));
	}

	rc = nvs_read(&fs, SETTINGS_LED_SIGNALIZATION_ID, &settings.enable_led_signalization, sizeof(settings.enable_led_signalization));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", DEFAULT_SETTINGS_ENABLE_LED_SIGNALIZATION, settings.enable_led_signalization);
	} 
	else   
	{/* item was not found, add it */
		settings.enable_led_signalization = DEFAULT_SETTINGS_ENABLE_LED_SIGNALIZATION;
		LOG_WRN("No settings found, set default %d", settings.enable_led_signalization);
		(void)nvs_write(&fs, SETTINGS_LED_SIGNALIZATION_ID, &settings.enable_led_signalization, sizeof(settings.enable_led_signalization));
	}

	LOG_INF("SETTINGS light enable				= %d", settings.enable_light_intensity);
	LOG_INF("SETTINGS distance enable 			= %d", settings.enable_distance);
	LOG_INF("SETTINGS threshold_light_intensity	= %d", settings.threshold_light_intensity);
	LOG_INF("SETTINGS threshold_distance 		= %d", settings.threshold_distance);
	LOG_INF("SETTINGS led sig enable 			= %d", settings.enable_led_signalization);
}


void SETTINGS_save(void)
{
	(void)nvs_write(&fs, SETTINGS_DISTANCE_ID, 		  	&settings.threshold_distance, 			sizeof(settings.threshold_distance));
	(void)nvs_write(&fs, SETTINGS_LIGHT_ID, 		  	&settings.threshold_light_intensity, 	sizeof(settings.threshold_light_intensity));
	(void)nvs_write(&fs, SETTINGS_DISTANCE_ENABLE_ID, 	&settings.enable_distance, 				sizeof(settings.enable_distance));
	(void)nvs_write(&fs, SETTINGS_LIGHT_ENABLE_ID, 	  	&settings.enable_light_intensity, 		sizeof(settings.enable_light_intensity));
	(void)nvs_write(&fs, SETTINGS_LED_SIGNALIZATION_ID,	&settings.enable_led_signalization, 	sizeof(settings.enable_led_signalization));
}
