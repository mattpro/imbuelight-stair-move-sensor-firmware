
#ifndef __SETTINGS_H
#define __SETTINGS_H

#include <zephyr/kernel.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>



#define FIRMWARE_VERSION 							05 // means 0.5	


#define DEFAULT_SETTINGS_LIGHT_INTENSITY_THRESHOLD 		300
#define DEFAULT_SETTINGS_LIGHT_INTENSITY_ENABLE 		true
#define DEFAULT_SETTINGS_LIGHT_INTENSITY_OUT_INVERT 	false

#define DEFAULT_SETTINGS_PRESENCE_THRESHOLD 			250
#define DEFAULT_SETTINGS_PRESENCE_ENABLE 				true
#define DEFAULT_SETTINGS_PRESENCE_OUT_INVERT 			false

#define DEFAULT_SETTINGS_SIGNAL_OUT_LOGIC_FUNCTION 		OUT_LOGIC_AND
#define DEFAULT_SETTINGS_SIGNAL_OUT_INVERT 				false

#define DEFAULT_SETTINGS_LED_SIGNALIZATION_SRC 			LED_SIGNALIZATION_SRC_OUT_SIGNAL



#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)


#define SETTINGS_ID 			1


extern bool light_state_current;
extern bool present_state_current;
extern bool light_state;
extern bool present_state;
extern bool sensor_state;

extern bool led_state;

extern int16_t current_light;


extern struct settings_t settings;

enum led_signalization_src_t {
    LED_SIGNALIZATION_SRC_NONE = 0,
    LED_SIGNALIZATION_SRC_MOVE_SENSOR = 1,
    LED_SIGNALIZATION_SRC_LIGHT_SENSOR = 2,
    LED_SIGNALIZATION_SRC_OUT_SIGNAL = 3
};

enum lout_logic_t {
    OUT_LOGIC_NONE = 0,
	OUT_LOGIC_AND = 1,
	OUT_LOGIC_OR = 2,
};

struct settings_t{		
	bool 				presence_enable;
	uint16_t 			presence_threshold;
	bool 				presence_out_invert;

	bool 				light_intensity_enable;
	int16_t 			light_intensity_threshold;
	bool 				light_intensity_out_invert;

	enum lout_logic_t 	signal_out_logic_function;
	uint8_t 			signal_out_invert;

	enum led_signalization_src_t led_signalization_src;
};

void SETTINGS_init(void);
void SETTINGS_save(void);
void SETTINGS_load(void);
void SETTINGS_load_default(void);

#endif

