#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include "utils.h"
#include "adc.h"
#include "ir_sensor.h"
#include "settings.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);


#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

static struct bt_data ad[] = 
{
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

uint8_t nus_buffer[13];

bool connection_state = false;
bool save_setting_flag = false;
bool ble_connected_flag = false;
bool led_state = false;



int16_t current_light = 0;
bool light_state = false;
bool present_state = false;

bool new_sensor_state = false;
bool sensor_state = false;




static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_INF("Received data from: %s msg: %s len: %d", addr, data, len);

	if ( len < 6 )
	{
		LOG_ERR("Recive too short frame");
		return;
	}

	settings.enable_presence 			= data[0] > 0 ? true : false;
	settings.enable_light_intensity 	= data[1] > 0 ? true : false;
	settings.enable_led_signalization	= data[2] > 0 ? true : false;
	settings.threshold_presence     	= (uint16_t)(((uint16_t)data[3] << 8 ) | data[4] );
	settings.threshold_light_intensity	= (uint16_t)(((uint16_t)data[5] << 8 ) | data[6] );

	IR_SENSOR_set_new_threshold( settings.threshold_presence );

	save_setting_flag = true; 
	
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


static void bt_ready(int err)
{
	uint8_t addr_s[3];
	bt_addr_le_t addr = {0};
	size_t count = 1;

	bt_id_get(&addr, &count);
	addr_s[0] = addr.a.val[0] ^ addr.a.val[1];
	addr_s[1] = addr.a.val[2] ^ addr.a.val[3];
	addr_s[2] = addr.a.val[4] ^ addr.a.val[5];

	char device_name[24];

	sprintf(device_name, "Imbue Light Move %02X%02X%02X", addr_s[0], addr_s[1], addr_s[2]);
	
	ad[1].data = (const uint8_t*)device_name;
	ad[1].data_len = strlen((const char*)device_name);

	err = bt_nus_init(&nus_cb);
	if (err) 
	{
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) 
	{
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return;
	}
}



int16_t ir_sensor_raw = 0;

int main(void)
{	
    LOG_INF("##############################"); 
    LOG_INF("##       Imbue Light        ##");
	LOG_INF("##     Move Controller      ##");	
	LOG_INF("##        ver 3.0           ##");
    LOG_INF("##   %s %s   ##", __DATE__, __TIME__);
    LOG_INF("##############################\n");

	LED_init();
	OUT_init();
	LED_start();
	SETTINGS_init();
	SETTINGS_load();
	SETTINGS_load_default();
	ADC_init();
	IR_SENSOR_init();

	int err = bt_enable(bt_ready);
	if (err) 
	{
		LOG_INF("Bluetooth init failed (err %d)\n", err);
	}

	while(1)
	{
		k_msleep(100);
		
		ir_sensor_raw = IR_SENSOR_get_raw();
		LOG_INF("LIGHT raw: %4d state: %d MOV raw: %4d state %d", current_light, light_state, ir_sensor_raw, present_state);
	}


	return 0;
}
