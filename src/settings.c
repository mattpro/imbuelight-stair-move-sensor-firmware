#include <zephyr/kernel.h>
#include <nrfx_saadc.h>
#include "settings.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(settings, CONFIG_LOG_DEFAULT_LEVEL);

struct settings_t settings;

static struct nvs_fs fs;

void SETTINGS_load_default(void)
{
	settings.motion_enable 				= DEFAULT_SETTINGS_MOTION_ENABLE;
	settings.motion_threshold 			= DEFAULT_SETTINGS_MOTION_THRESHOLD;
	settings.motion_out_invert 			= DEFAULT_SETTINGS_MOTION_OUT_INVERT;

    settings.presence_threshold 		= DEFAULT_SETTINGS_PRESENCE_THRESHOLD;
    settings.presence_enable 			= DEFAULT_SETTINGS_PRESENCE_ENABLE;
    settings.presence_out_invert 		= DEFAULT_SETTINGS_PRESENCE_OUT_INVERT;

    settings.light_intensity_threshold 	= DEFAULT_SETTINGS_LIGHT_INTENSITY_THRESHOLD;
    settings.light_intensity_enable 	= DEFAULT_SETTINGS_LIGHT_INTENSITY_ENABLE;
    settings.light_intensity_out_invert = DEFAULT_SETTINGS_LIGHT_INTENSITY_OUT_INVERT;

    settings.signal_out_logic_function 	= DEFAULT_SETTINGS_SIGNAL_OUT_LOGIC_FUNCTION;
    settings.signal_out_invert 			= DEFAULT_SETTINGS_SIGNAL_OUT_INVERT;

    settings.led_signalization_src 		= DEFAULT_SETTINGS_LED_SIGNALIZATION_SRC;
}

void SETTINGS_print_all(void)
{
	// Print all setting from settings struct
	k_msleep(100);
	LOG_WRN("SETTINGS light enable              = %d", settings.light_intensity_enable);
	k_msleep(100);
	LOG_WRN("SETTINGS threshold_light_intensity = %d", settings.light_intensity_threshold);
	k_msleep(100);
	LOG_WRN("SETTINGS light_intensity_out_invert= %d", settings.light_intensity_out_invert);
	k_msleep(100);
	LOG_WRN("SETTINGS presence enable           = %d", settings.presence_enable);
	k_msleep(100);
	LOG_WRN("SETTINGS threshold_prescence       = %d", settings.presence_threshold);
	k_msleep(100);
	LOG_WRN("SETTINGS presence_out_invert       = %d", settings.presence_out_invert);
	k_msleep(100);
	LOG_WRN("SETTINGS signal_out_logic_function = %d", settings.signal_out_logic_function);
	k_msleep(100);
	LOG_WRN("SETTINGS signal_out_invert         = %d", settings.signal_out_invert);
	k_msleep(100);
	LOG_WRN("SETTINGS led_signalization_src     = %d", settings.led_signalization_src);
	k_msleep(100);
	LOG_WRN("SETTINGS motion enable             = %d", settings.motion_enable);
	k_msleep(100);
	LOG_WRN("SETTINGS threshold_motion          = %d", settings.motion_threshold);
	k_msleep(100);
	LOG_WRN("SETTINGS motion_out_invert         = %d", settings.motion_out_invert);
	k_msleep(100);
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
		return;
	}
	fs.offset = NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
		return;
	}
	fs.sector_size = info.size;
	fs.sector_count = 4U;

	rc = nvs_mount(&fs);
	if (rc) {
		printk("Flash Init failed\n");
		return;
	}
}


void SETTINGS_load(void)
{
	int rc = 0;

	LOG_INF("LOAD SETTINGS !");
	rc = nvs_read(&fs, SETTINGS_ID, &settings, sizeof(settings));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d Recived: %d bytes", SETTINGS_ID, rc);
	} 
	else   
	{/* item was not found, add it */
		SETTINGS_load_default();
		LOG_WRN("No settings found, set default");
		(void)nvs_write(&fs, SETTINGS_ID, &settings, sizeof(settings));
	}

	SETTINGS_print_all();
}


void SETTINGS_save(void)
{
	LOG_INF("SAVE SETTINGS !");

	int rc = nvs_write(&fs, SETTINGS_ID, &settings, sizeof(settings));
	if (rc < 0) 
	{
		LOG_ERR("Problem writing settings to flash");
	}

	SETTINGS_print_all();
}
