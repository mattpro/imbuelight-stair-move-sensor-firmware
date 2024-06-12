
#ifndef __SETTINGS_H
#define __SETTINGS_H

#include <zephyr/kernel.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>


#define FIRMWARE_VERSION 							03 // means 0.3	

#define DEFAULT_SETTINGS_THRESHOLD_PRESENCE 		350
#define DEFAULT_SETTINGS_THRESHOLD_LIGHT_INTENSITY 	300
#define DEFAULT_SETTINGS_ENABLE_PRESENCE 			true
#define DEFAULT_SETTINGS_ENABLE_LIGHT_INTENSITY 	true
#define DEFAULT_SETTINGS_ENABLE_LED_SIGNALIZATION 	true


#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define SETTINGS_PRESENCE_ID 			1
#define SETTINGS_LIGHT_ID				2
#define SETTINGS_PRESENCE_ENABLE_ID		3
#define SETTINGS_LIGHT_ENABLE_ID		4
#define SETTINGS_LED_SIGNALIZATION_ID	5


extern int16_t current_light;
extern bool light_state;
extern bool present_state;
extern bool new_sensor_state;
extern bool sensor_state;

extern struct settings_t settings;

struct settings_t{
	uint16_t threshold_presence;
	int16_t threshold_light_intensity;
	bool 	enable_presence;
	bool 	enable_light_intensity;
    bool    enable_led_signalization;
};

void SETTINGS_init(void);
void SETTINGS_save(void);
void SETTINGS_load(void);
void SETTINGS_load_default(void);

#endif

