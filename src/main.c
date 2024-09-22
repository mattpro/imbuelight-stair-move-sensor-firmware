#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <stdio.h>
#include "utils.h"
#include "adc.h"
#include "ir_sensor.h"
#include "settings.h"
#include "logic.h"

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
bool set_new_threshold_flag = false;


int16_t current_light = 0;
int16_t ir_sensor_raw = 0;

// Direct signal from sensro
bool light_state_current = false;
bool present_state_current = false;

// this can be inverted or no 
bool light_state = false;
bool present_state = false;

// this is final state
bool sensor_state = false;

bool led_signalization_connected_flag = false;
bool led_signalization_disconnect_flag = false;
bool led_signalization_save_flag = false;


#define CMD_SET_CONFIG 	0xEC
#define CMD_GET_CONFIG 	0xBC
#define CMD_SEND_DATA 	0x0D



static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};
	uint8_t cmd = 0x00;
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_INF("Received data from: %s msg: %s len: %d", addr, data, len);

	for (int i = 0 ; i < len ; i ++ )
	{
		LOG_INF("[%2d]=%02X", i, data[i]);
	}	

	cmd = data[0];

	switch(cmd)
	{
		case CMD_SET_CONFIG:
			LOG_INF("CMD_SET_CONFIG");

			settings.light_intensity_enable = data[1] > 0 ? true : false;
			settings.light_intensity_threshold = (uint16_t)(((uint16_t)data[3] << 8 ) | data[2] );
			settings.light_intensity_out_invert = data[4] > 0 ? true : false;

			settings.presence_enable = data[5] > 0 ? true : false;
			settings.presence_threshold = (uint16_t)(((uint16_t)data[7] << 8 ) | data[6] );
			settings.presence_out_invert = data[8] > 0 ? true : false;

			settings.signal_out_logic_function = (enum lout_logic_t)data[9];
			settings.signal_out_invert = data[10] > 0 ? true : false;

			settings.led_signalization_src =  (enum led_signalization_src_t)data[11];

			set_new_threshold_flag = true;
			
			led_signalization_save_flag = true;
		break;
		case CMD_GET_CONFIG:
			LOG_INF("CMD_GET_CONFIG");

			nus_buffer[0] = CMD_GET_CONFIG;
			nus_buffer[1] = settings.light_intensity_enable;
			nus_buffer[2] = settings.light_intensity_threshold >> 8;
			nus_buffer[3] = settings.light_intensity_threshold & 0xFF;
			nus_buffer[4] = settings.light_intensity_out_invert;
			nus_buffer[5] = settings.presence_enable;
			nus_buffer[6] = settings.presence_threshold >> 8;
			nus_buffer[7] = settings.presence_threshold & 0xFF;
			nus_buffer[8] = settings.presence_out_invert;
			nus_buffer[9] = (uint8_t)settings.signal_out_logic_function;
			nus_buffer[10] = settings.signal_out_invert;
			nus_buffer[11] = (uint8_t)settings.led_signalization_src;
			nus_buffer[12] = FW_VERSION;

			bt_nus_send(NULL, nus_buffer, 13);
		break;
		default:
			LOG_ERR("CMD - Unknown command");
		break;
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
	led_signalization_connected_flag = true;
}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	connection_state = false;
	led_signalization_disconnect_flag = true;
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
	char device_name_adv[24];

	bt_id_get(&addr, &count);
	addr_s[0] = addr.a.val[0] ^ addr.a.val[1];
	addr_s[1] = addr.a.val[2] ^ addr.a.val[3];
	addr_s[2] = addr.a.val[4] ^ addr.a.val[5];

	sprintf(device_name_adv, "IL Move %02X%02X%02X", addr_s[0], addr_s[1], addr_s[2]);
	
	LOG_INF("ADV NAME: %s", device_name_adv);

	ad[1].data = (const uint8_t*)device_name_adv;
	ad[1].data_len = strlen((const char*)device_name_adv);

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


void BOOT_info(void)
{
    LOG_INF("##############################"); 
    LOG_INF("##       Imbue Light        ##");
	LOG_INF("##     Move Controller      ##");	
	LOG_INF("##        ver 3.0           ##");
    LOG_INF("##   %s %s   ##", __DATE__, __TIME__);
    LOG_INF("##############################\n");
}


int main(void)
{	
	BOOT_info();
	LED_init();
	OUT_init();
	LED_start();
	SETTINGS_init();
	SETTINGS_load();
	//SETTINGS_load_default();
	ADC_init();
	IR_SENSOR_init();

	int err = bt_enable(bt_ready);
	if (err) 
	{
		LOG_INF("Bluetooth init failed (err %d)\n", err);
	}

	while(1)
	{
		if ( connection_state == true )
		{
			ir_sensor_raw = IR_SENSOR_get_raw();

			// IR_SENSOR_get_all_raw_data(&IR_SENSOR_raw_data);

			// LOG_INF("object: %4d | ambient: %4d | obj_comp: %4d | presence: %4d | motion: %4d | shock: %4d | presence_abs: %4d", 
			// 		IR_SENSOR_raw_data.object_raw, 
			// 		IR_SENSOR_raw_data.object_raw, 
			// 		IR_SENSOR_raw_data.tambient_raw, 
			// 		IR_SENSOR_raw_data.tobj_comp_raw, 
			// 		IR_SENSOR_raw_data.tpresence_raw, 
			// 		IR_SENSOR_raw_data.tmotion_raw, 
			// 		IR_SENSOR_raw_data.tamb_shock_raw, 
			// 		IR_SENSOR_raw_data.presence_abs_value);

			//LOG_INF("LIGHT raw: %4d state: %d MOV raw: %4d state %d", current_light, light_state, ir_sensor_raw, present_state);

			nus_buffer[0] = CMD_SEND_DATA;

			nus_buffer[1] = current_light >> 8;
			nus_buffer[2] = current_light & 0xFF;
			nus_buffer[3] = light_state;
			nus_buffer[4] = ir_sensor_raw >> 8;
			nus_buffer[5] = ir_sensor_raw & 0xFF;
			nus_buffer[6] = present_state;
			nus_buffer[7] = sensor_state;

			bt_nus_send(NULL, nus_buffer, 8);
		}

		if ( led_signalization_connected_flag )
		{
			led_signalization_connected_flag = false;
 			LED_connected_signalization();
		}
		if ( led_signalization_disconnect_flag )
		{
 			LED_disconnect_signalization();
			led_signalization_disconnect_flag = false;
		}
		if ( led_signalization_save_flag )
		{
			SETTINGS_save();
 			LED_save_signalization();
			led_signalization_save_flag = false;
		}
		if ( set_new_threshold_flag )
		{
			set_new_threshold_flag = false;
			IR_SENSOR_set_new_threshold( settings.presence_threshold );
			//IR_SENSOR_reset();
			// TODO: Led signalization to indicate save settings
			light_state_current = false;
			present_state_current = false;
			light_state = false;
			present_state = false;
			sensor_state = false;
			led_state = false;
			OUT_set(sensor_state); 
			LED_set(led_state);
			LOGIC_signal();
		}


		k_msleep(100);	
	}

	return 0;
}
