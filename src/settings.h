
#ifndef __SETTINGS_H
#define __SETTINGS_H

#include <zephyr/kernel.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>


#define DEFAULT_SETTINGS_THRESHOLD_DISTANCE 		80
#define DEFAULT_SETTINGS_THRESHOLD_LIGHT_INTENSITY 	200
#define DEFAULT_SETTINGS_ENABLE_DISTANCE 			true
#define DEFAULT_SETTINGS_ENABLE_LIGHT_INTENSITY 	false
#define DEFAULT_SETTINGS_ENABLE_LED_SIGNALIZATION 	true


#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define SETTINGS_DISTANCE_ID 			1
#define SETTINGS_LIGHT_ID				2
#define SETTINGS_DISTANCE_ENABLE_ID		3
#define SETTINGS_LIGHT_ENABLE_ID		4
#define SETTINGS_LED_SIGNALIZATION_ID	5


extern struct settings_t settings;

struct settings_t{
	uint8_t threshold_distance;
	int16_t threshold_light_intensity;
	bool 	enable_distance;
	bool 	enable_light_intensity;
    bool    enable_led_signalization;
};

void SETTINGS_init(void);
void SETTINGS_save(void);
void SETTINGS_load(void);
void SETTINGS_load_default(void);

#endif

