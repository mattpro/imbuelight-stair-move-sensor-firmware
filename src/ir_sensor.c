#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>


#include "sths34pf80_reg.h"
#include "ir_sensor.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ir_sensor, CONFIG_LOG_DEFAULT_LEVEL);


#define SW0_NODE		DT_ALIAS(irsensorint)
#define I2C_DEV_NODE	DT_ALIAS(i2c0)


static const struct gpio_dt_spec ir_sensor_int = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

static struct gpio_callback irsensor_int_cb_data;

volatile sths34pf80_drdy_status_t status;
volatile sths34pf80_func_status_t func_status;

stmdev_ctx_t ir_sensor; 

static int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len);
static int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len);
static void platform_delay(uint32_t ms);

extern void ir_sensor_detect_work_handler(struct k_work *work);
K_WORK_DEFINE(ir_sensor_detect_work, ir_sensor_detect_work_handler);



void irsensor_int_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("IR Sensor interupt %" PRIu32, k_cycle_get_32());
	int gpio_state = 0;

	gpio_state = gpio_pin_get_dt(&ir_sensor_int);
	LOG_INF("Status: %02x", gpio_state);

	if ( gpio_state == 1 )
	{
		set_led(0);
		set_out(0);
	}
	else
	{
		set_led(1);
		set_out(1);
	}

 //	k_work_submit(&ir_sensor_detect_work);
}



void ir_sensor_detect_work_handler(struct k_work *work)
{
	uint16_t raw;
	static uint32_t cnt = 0;

	int gpio_state = 0;


	gpio_state = gpio_pin_get_dt(&ir_sensor_int);
	LOG_INF("Status: %02x", gpio_state);


	set_led(gpio_state);
	set_out(gpio_state);

	// sths34pf80_drdy_status_get(&ir_sensor, &status);
 	// LOG_INF("Status: %02x", status.drdy);

 	// if (status.drdy)
	// {
 	//  	sths34pf80_func_status_get(&ir_sensor, &func_status);
	//  	sths34pf80_tobject_raw_get(&ir_sensor, &raw);
 	//  	if ((cnt++ % 30) == 0)
 	//  	{
 	//  		LOG_INF("-->TA %d - P %d - M %d RAW: %d", func_status.tamb_shock_flag, func_status.pres_flag, func_status.mot_flag, raw);
 	//  	}
	// 	set_led(func_status.pres_flag);
	// 	set_out(func_status.pres_flag);
 	// }
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
		while(1);
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

	sths34pf80_presence_threshold_set(&ir_sensor, 350); // less - more sensitve

	/* Set ODR */
	sths34pf80_odr_set(&ir_sensor, STHS34PF80_ODR_AT_30Hz);
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
