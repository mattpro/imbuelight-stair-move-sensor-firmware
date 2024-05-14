#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
//#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include "utils.h"
//#include "adc.h"
//#include "distance_sensor.h"
#include "settings.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);


#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

uint8_t nus_buffer[13];
uint16_t distance = 200;
uint8_t distance_err = 0;
uint16_t new_distance = 0;
int16_t light = 2000;
bool sensor_state = false;
bool connection_state = false;
bool save_setting_flag = false;
bool ble_connected_flag = false;
bool led_state = false;
static uint8_t counter_led_connection = 0;

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

	settings.enable_distance 			= data[0] > 0 ? true : false;
	settings.enable_light_intensity 	= data[1] > 0 ? true : false;
	settings.enable_led_signalization	= data[2] > 0 ? true : false;
	settings.threshold_distance     	= (uint16_t)(((uint16_t)data[3] << 8 ) | data[4] );
	settings.threshold_light_intensity	= (uint16_t)(((uint16_t)data[5] << 8 ) | data[6] );

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

#include <zephyr/drivers/i2c.h>

//##if DT_NODE_HAS_STATUS(DT_ALIAS(i2c_0), okay)
#define I2C_DEV_NODE	DT_ALIAS(i2c0)
//#endif


#define I2C_ADD 0x29

const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);


#define    BOOT_TIME         10 //ms
#include "sths34pf80_reg.h"
/** Please note that is MANDATORY: return 0 -> no Error.**/
int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len);
int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len);


volatile sths34pf80_drdy_status_t status;
volatile sths34pf80_func_status_t func_status;

int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len)
{
	int a,b;
	uint8_t buff[100];

	buff[0] = Reg;
	memcpy(&buff[1], Bufp, len);
	a = i2c_write(i2c_dev, buff, 1+len, 0x5A);
	return a;// + b;
}

int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len)
{
	int a,b;
	uint8_t buff[2];

	buff[0] = Reg;
	a = i2c_write(i2c_dev, buff, 1, 0x5A);
	b = i2c_read(i2c_dev, Bufp, len, 0x5A);
	return a + b;
}

static void platform_delay(uint32_t ms)
{
	k_msleep(ms);
}




void main(void)
{	
	int err;
	int ret;

    LOG_INF("##############################"); 
    LOG_INF("##       Imbue Light        ##");
	LOG_INF("##     Move Controller      ##");	
	LOG_INF("##        ver 1.1           ##");
    LOG_INF("##   %s %s   ##", __DATE__, __TIME__);
    LOG_INF("##############################\n");

	led_init();
	led_start();
	out_init();
	//adc_init();
	//distance_sensor_init();
	led_start();

	err = bt_enable(bt_ready);
	if (err) 
	{
		LOG_INF("Bluetooth init failed (err %d)\n", err);
	}



	
	uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
	uint32_t i2c_cfg_tmp;

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device is not ready\n");
	}

	/* 1. Verify i2c_configure() */
	if (i2c_configure(i2c_dev, i2c_cfg)) {
		LOG_ERR("I2C config failed\n");

	}


	uint8_t read_buff[10];
	uint8_t write_buff[20];
	int stat1;
	int stat2;


	int stat;
	//for ( int i = 0 ; i < 127 ; i ++ )
	//{
	//	stat = i2c_read(i2c_dev, read_buff, 1, i);
	//	LOG_INF("add=%02x = %d", i , stat);
	//	k_msleep(5);
	//}



	// write_buff[0] = (uint8_t)(STHS34PF80_WHO_AM_I);
	// stat1 = i2c_write(i2c_dev, write_buff, 1, 0x5A);
	// stat2 = i2c_read(i2c_dev,read_buff, 1, 0x5A);
	// LOG_INF("READ: %02X", read_buff[0]);



	uint8_t whoami;
  	sths34pf80_lpf_bandwidth_t lpf_m, lpf_p, lpf_p_m, lpf_a_t;


	stmdev_ctx_t dev_ctx; /** xxxxxxx is the used part number **/
	dev_ctx.write_reg = platform_write;
	dev_ctx.read_reg = platform_read;
	dev_ctx.mdelay = platform_delay;

	/* Wait sensor boot time */
	platform_delay(BOOT_TIME);

	/* Check device ID */
	sths34pf80_device_id_get(&dev_ctx, &whoami);
	if (whoami != STHS34PF80_ID)
	{
		LOG_ERR("DEVICE WRONG ID: %X", whoami);
		while(1);
	}
	else
	{
		LOG_INF("DEVICE OK");
	}


	/* Set averages (AVG_TAMB = 8, AVG_TMOS = 32) */
	sths34pf80_avg_tobject_num_set(&dev_ctx, STHS34PF80_AVG_TMOS_32);
	sths34pf80_avg_tambient_num_set(&dev_ctx, STHS34PF80_AVG_T_8);
	/* read filters */
	sths34pf80_lpf_m_bandwidth_get(&dev_ctx, &lpf_m);
	sths34pf80_lpf_p_bandwidth_get(&dev_ctx, &lpf_p);
	sths34pf80_lpf_p_m_bandwidth_get(&dev_ctx, &lpf_p_m);
	sths34pf80_lpf_a_t_bandwidth_get(&dev_ctx, &lpf_a_t);
	LOG_INF("lpf_m: %02d, lpf_p: %02d, lpf_p_m: %02d, lpf_a_t: %02d\r\n", lpf_m, lpf_p, lpf_p_m, lpf_a_t);
	/* Set BDU */
	sths34pf80_block_data_update_set(&dev_ctx, 1);
	/* Set ODR */
	sths34pf80_odr_set(&dev_ctx, STHS34PF80_ODR_AT_30Hz);


	LOG_INF("START");
	int32_t cnt = 0;


	/* Presence event detected in irq handler */
	while(1) 
	{
		k_msleep(1);
		sths34pf80_drdy_status_get(&dev_ctx, &status);
		//LOG_INF("Status: %02x", status.drdy);
		if (status.drdy)
		{
			sths34pf80_func_status_get(&dev_ctx, &func_status);
			//if ((cnt++ % 30) == 0)
			//{
				LOG_INF("-->TA %d - P %d - M %d\r\n", func_status.tamb_shock_flag, func_status.pres_flag, func_status.mot_flag);
				
			//}

			set_led(func_status.mot_flag);
			set_out(func_status.mot_flag);
		}
	}
}
