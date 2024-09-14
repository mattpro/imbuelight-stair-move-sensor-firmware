#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>


#include "sths34pf80_reg.h"
#include "settings.h"
#include "utils.h"
#include "ir_sensor.h"
#include "logic.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ir_sensor, CONFIG_LOG_DEFAULT_LEVEL);


#define IRSENSOR_INT_NODE	DT_ALIAS(irsensorint)
#define I2C_DEV_NODE		DT_ALIAS(i2c0)


static const struct gpio_dt_spec ir_sensor_int = GPIO_DT_SPEC_GET_OR(IRSENSOR_INT_NODE, gpios, {0});
static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

static struct gpio_callback irsensor_int_cb_data;

volatile sths34pf80_drdy_status_t status;
volatile sths34pf80_func_status_t func_status;

stmdev_ctx_t ir_sensor; 

IR_SENSOR_raw_data_t IR_SENSOR_raw_data;

static int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len);
static int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len);
static void platform_delay(uint32_t ms);

// TODO:
// https://github.com/STMicroelectronics/X-CUBE-MEMS1/blob/main/Middlewares/ST/STM32_InfraredPD_Library/Lib/InfraredPD_CM0P_wc16_ot.a


void irsensor_int_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int gpio_state = 0;
	uint8_t old_present_state = false;

	gpio_state = gpio_pin_get_dt(&ir_sensor_int);
	LOG_INF("Status: %02x", !gpio_state);
	
	if ( settings.presence_enable == true )
	{
		old_present_state = present_state_current;
		present_state_current = !gpio_state;
		if ( present_state_current != old_present_state )
		{
			LOG_INF("Presence state changed - current: %d", present_state_current);
			LOGIC_signal();
		}
	}
	else
	{
		present_state = true;
	}
}


int IR_SENSOR_init(void)
{
	uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
	uint8_t whoami;
  	sths34pf80_lpf_bandwidth_t lpf_m, lpf_p, lpf_p_m, lpf_a_t;
	int ret;


	if (!device_is_ready(i2c_dev)) 
	{
		LOG_ERR("I2C device is not ready\n");
	}

	if (i2c_configure(i2c_dev, i2c_cfg)) 
	{
		LOG_ERR("I2C config failed\n");
	}

	ir_sensor.write_reg = platform_write;
	ir_sensor.read_reg = platform_read;
	ir_sensor.mdelay = platform_delay;

	/* Set interupt pin */
	if (!gpio_is_ready_dt(&ir_sensor_int)) 
	{
		LOG_ERR("Error: button device %s is not ready", ir_sensor_int.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&ir_sensor_int, GPIO_INPUT);
	if (ret != 0) 
	{
		LOG_ERR("Error %d: failed to configure %s pin %d", ret, ir_sensor_int.port->name, ir_sensor_int.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&ir_sensor_int, GPIO_INT_EDGE_BOTH);
	if (ret != 0) 
	{
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret, ir_sensor_int.port->name, ir_sensor_int.pin);
		return 0;
	}

	gpio_init_callback(&irsensor_int_cb_data, irsensor_int_cb, BIT(ir_sensor_int.pin));
	gpio_add_callback(ir_sensor_int.port, &irsensor_int_cb_data);
	LOG_INF("Set up button at %s pin %d", ir_sensor_int.port->name, ir_sensor_int.pin);


	/* Wait sensor boot time */
	platform_delay(BOOT_TIME);

	/* Check device ID */
	sths34pf80_device_id_get(&ir_sensor, &whoami);
	if (whoami != STHS34PF80_ID)
	{
		LOG_ERR("DEVICE WRONG ID: %X", whoami);
		//while(1);
	}
	else
	{
		LOG_INF("DEVICE OK");
	}

	/* Set averages (AVG_TAMB = 8, AVG_TMOS = 32) */
	sths34pf80_avg_tobject_num_set(&ir_sensor, STHS34PF80_AVG_TMOS_32);
	sths34pf80_avg_tambient_num_set(&ir_sensor, STHS34PF80_AVG_T_8);

	sths34pf80_lpf_m_bandwidth_set(&ir_sensor, STHS34PF80_LPF_ODR_DIV_800); //MONTION

	/* read filters */
	sths34pf80_lpf_m_bandwidth_get(&ir_sensor, &lpf_m); //4 - ODR/200 - MONTION
	sths34pf80_lpf_p_bandwidth_get(&ir_sensor, &lpf_p); //4 - ODR/200 - PRESENT
	sths34pf80_lpf_p_m_bandwidth_get(&ir_sensor, &lpf_p_m); //0 - ODR/9 - BOTH
	sths34pf80_lpf_a_t_bandwidth_get(&ir_sensor, &lpf_a_t); //2 - ODR/50
	LOG_INF("lpf_m: %02d, lpf_p: %02d, lpf_p_m: %02d, lpf_a_t: %02d\r\n", lpf_m, lpf_p, lpf_p_m, lpf_a_t);
	/* Set BDU */
	sths34pf80_block_data_update_set(&ir_sensor, 1);	
	/* Set intreupt */
	sths34pf80_int_or_set(&ir_sensor, STHS34PF80_INT_PRESENCE);
	sths34pf80_route_int_set(&ir_sensor, STHS34PF80_INT_OR);

	sths34pf80_presence_abs_value_set(&ir_sensor, 1);
	/* Set threshold from settings */
	sths34pf80_presence_threshold_set(&ir_sensor, settings.presence_threshold); // less - more sensitve
	/* Set ODR */
	sths34pf80_odr_set(&ir_sensor, STHS34PF80_ODR_AT_8Hz);

	return 1;
}


void IR_SENSOR_set_new_threshold(uint16_t threshold)
{
	sths34pf80_presence_threshold_set(&ir_sensor, threshold); 
}


int IR_SENSOR_get_all_raw_data(IR_SENSOR_raw_data_t* IR_SENSOR_raw_data) 
{
	sths34pf80_tobject_raw_get(&ir_sensor, &IR_SENSOR_raw_data->object_raw);
	sths34pf80_tambient_raw_get(&ir_sensor, &IR_SENSOR_raw_data->tambient_raw);
	sths34pf80_tobj_comp_raw_get(&ir_sensor, &IR_SENSOR_raw_data->tobj_comp_raw);
	sths34pf80_tpresence_raw_get(&ir_sensor, &IR_SENSOR_raw_data->tpresence_raw);
	sths34pf80_tmotion_raw_get(&ir_sensor, &IR_SENSOR_raw_data->tmotion_raw);
	sths34pf80_tamb_shock_raw_get(&ir_sensor, &IR_SENSOR_raw_data->tamb_shock_raw);
	sths34pf80_presence_abs_value_get(&ir_sensor, &IR_SENSOR_raw_data->presence_abs_value);

    return 0; // Success
}


int16_t IR_SENSOR_get_raw(void)
{
	int16_t raw;

	sths34pf80_tobject_raw_get(&ir_sensor, &raw);
	return raw;
}


void IR_SENSOR_reset(void)
{
	sths34pf80_algo_reset(&ir_sensor);
}

static int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len)
{
	int a;
	uint8_t buff[100];

	buff[0] = Reg;
	memcpy(&buff[1], Bufp, len);
	a = i2c_write(i2c_dev, buff, 1+len, 0x5A);
	return a;
}


static int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len)
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
