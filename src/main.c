#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include "utils.h"
#include "adc.h"
#include "distance_sensor.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);


#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define SETTINGS_DISTANCE_ID 	1
#define SETTINGS_LIGHT_ID		2


static struct nvs_fs fs;



static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};


uint8_t settings_distance_cm = 80;
int16_t settings_light_desinty = 200;


uint8_t nus_buffer[25];
int distance = 0;
int light = 0;
bool sensor_state = false;
bool connection_state = false;

bool save_setting_distance_flag = false;
bool save_setting_light_flag = false;



static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_INF("Received data from: %s msg: %s len: %d", addr, data, len);

	if ( ( data[0] == 'd' ) && ( len >= 4 ) )
	{
		settings_distance_cm = (data[1] - 48)*100 + (data[2] - 48)*10 + (data[3] - 48); 
		LOG_INF("Set new distance: %d", settings_distance_cm);
 		save_setting_distance_flag = true; 
	}
	if ( ( data[0] == 'l' ) || ( data[4] == 'l' ) )
	{
		settings_light_desinty = (data[5] - 48)*1000 + (data[6] - 48)*100 + (data[7] - 48)*10 + (data[8] - 48) ; 
		LOG_INF("Set new light: %d", settings_light_desinty);
		save_setting_light_flag = true;
	}
	
	for (int i = 0 ; i < len ; i ++ )
	{
		LOG_INF("[%2d]=%02X", i, data[i]);
	}	
}


static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	if (err) 
	{
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", addr);
	//current_conn = bt_conn_ref(conn);

	connection_state = true;
}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	connection_state = false;
}


static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};


BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
};


void main(void)
{	
	int err;

	LOG_INF("Imbue light move controller ver 1.0 start");
	led_init();
	led_start();
	out_init();
	adc_init();
	distance_sensor_init();
	led_start();

	err = bt_enable(NULL);
	if (err) 
	{
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	err = bt_nus_init(&nus_cb);
	if (err) 
	{
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return 0;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) 
	{
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return 0;
	}



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


	rc = nvs_read(&fs, SETTINGS_DISTANCE_ID, &settings_distance_cm, sizeof(settings_distance_cm));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", SETTINGS_DISTANCE_ID, settings_distance_cm);
	} 
	else   
	{/* item was not found, add it */
		settings_distance_cm = 80;
		LOG_WRN("No settings found, set default %d cm", settings_distance_cm);
		(void)nvs_write(&fs, SETTINGS_DISTANCE_ID, &settings_distance_cm, strlen(settings_distance_cm));
	}

	rc = nvs_read(&fs, SETTINGS_LIGHT_ID, &settings_light_desinty, sizeof(settings_light_desinty));
	if (rc > 0) 
	{ /* item was found, show it */
		LOG_INF("Id: %d, Address: %s\n", SETTINGS_LIGHT_ID, settings_light_desinty);
	} 
	else   
	{/* item was not found, add it */
		settings_light_desinty = 200;
		LOG_WRN("No settings found, set default %d", settings_light_desinty);
		(void)nvs_write(&fs, SETTINGS_LIGHT_ID, &settings_light_desinty, strlen(settings_light_desinty));
	}


	while (1) 
	{
		distance = get_distance();
		light = get_light_intensity();

		LOG_INF("distance: %4dcm max: %4dcm || light: %4d max: %4d", distance, settings_distance_cm, light, settings_light_desinty); 

		if ( ( distance < settings_distance_cm ) && ( light < settings_light_desinty ) )
		{
			sensor_state = true;
		}
		else
		{
			sensor_state = false;
		}

		set_led(sensor_state);
		set_out(sensor_state);

		sprintf(nus_buffer, "s:%c d:%3d l:%4d", sensor_state? '1' : '0', distance, light); 
		if ( connection_state )
		{
			if (bt_nus_send(NULL, nus_buffer, strlen(nus_buffer))) 
			{
			//	LOG_WRN("Failed to send data over BLE connection");
			}
		}

		if ( save_setting_distance_flag )
		{
			save_setting_distance_flag = false;
			(void)nvs_write(&fs, SETTINGS_DISTANCE_ID, &settings_distance_cm, strlen(settings_distance_cm));
		}

		if ( save_setting_light_flag )
		{
			save_setting_light_flag = false;
			(void)nvs_write(&fs, SETTINGS_LIGHT_ID, &settings_light_desinty, strlen(settings_light_desinty));
		}


		k_msleep(50);
	}
}
