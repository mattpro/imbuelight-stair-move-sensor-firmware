// ir_sensor.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>   // atomic_t / atomic ops (anti-flood dla workqueue)
#include <zephyr/sys/util.h>     // ARG_UNUSED
#include <errno.h>               // -EINVAL

#include <string.h>              // memcpy
#include <stdbool.h>

#include "sths34pf80_reg.h"
#include "settings.h"
#include "utils.h"
#include "ir_sensor.h"
#include "logic.h"

LOG_MODULE_REGISTER(ir_sensor, CONFIG_LOG_DEFAULT_LEVEL);

/* -------------------------------------------------------------------------- */
/* Devicetree aliases                                                         */
/* -------------------------------------------------------------------------- */
#define IRSENSOR_INT_NODE  DT_ALIAS(irsensorint)
#define I2C_DEV_NODE       DT_ALIAS(i2c0)

/* I2C adres czujnika */
#ifndef IRSENSOR_I2C_ADDR
#define IRSENSOR_I2C_ADDR  0x5A
#endif


/* -------------------------------------------------------------------------- */
/* DT bindings                                                                */
/* -------------------------------------------------------------------------- */
static const struct gpio_dt_spec ir_sensor_int = GPIO_DT_SPEC_GET_OR(IRSENSOR_INT_NODE, gpios, {0});
static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

/* -------------------------------------------------------------------------- */
/* Stan modułu                                                                */
/* -------------------------------------------------------------------------- */
static struct gpio_callback irsensor_int_cb_data;

/* globalne – jeśli czytane gdzie indziej */
volatile sths34pf80_drdy_status_t status;
volatile sths34pf80_func_status_t func_status;

stmdev_ctx_t ir_sensor;

IR_SENSOR_raw_data_t IR_SENSOR_raw_data;

/* Workqueue do obsługi przerwania (ISR robi tylko submit) */
static struct k_work ir_int_work;
static atomic_t ir_work_pending = ATOMIC_INIT(0);

/* Te zmienne są zdefiniowane w main.c – używane tutaj: */
extern bool present_state_current;
extern bool motion_state_current;
extern bool present_state; /* istnieje w projekcie */
extern bool motion_state;  /* istnieje w projekcie */

/* -------------------------------------------------------------------------- */
/* Prototypy                                                                  */
/* -------------------------------------------------------------------------- */
static int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len);
static int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len);
static void platform_delay(uint32_t ms);

static void ir_int_work_handler(struct k_work *work);

/* -------------------------------------------------------------------------- */
/* ISR – minimalny: tylko zgłoszenie pracy                                    */
/* -------------------------------------------------------------------------- */
static void irsensor_int_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* Anti-flood: jedna praca w kolejce naraz */
    if (atomic_cas(&ir_work_pending, 0, 1)) {
        k_work_submit(&ir_int_work);
    }
}

/* -------------------------------------------------------------------------- */
/* Handler pracy – w kontekście wątku systemowego                              */
/* -------------------------------------------------------------------------- */
static void ir_int_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    /* Zezwól ISR na zgłoszenie kolejnej pracy */
    atomic_clear(&ir_work_pending);

    /* Odczyt/wyczyszczenie flag statusu w sensorze (side-effect: kasuje IRQ) */
    sths34pf80_func_status_t fs = (sths34pf80_func_status_t){0};
    sths34pf80_drdy_status_t st = (sths34pf80_drdy_status_t){0};

    (void)sths34pf80_func_status_get(&ir_sensor, &fs);
    (void)sths34pf80_drdy_status_get(&ir_sensor, &st);

    /* Zapisz do globalnych (jeśli ktoś czyta gdzie indziej) */
    func_status = fs;
    status = st;

    bool motion_flag   = (fs.mot_flag != 0);
    bool presence_flag = (fs.pres_flag != 0);

    /* (opcjonalnie) diagnostyka pinu:
       int pin_level = gpio_pin_get_dt(&ir_sensor_int);
       LOG_DBG("INT pin=%d, mot=%d pres=%d", pin_level, motion_flag, presence_flag);
    */

    /* Motion */
    if (settings.motion_enable) {
        bool motion_old = motion_state_current;
        motion_state_current = motion_flag ? 1 : 0;

        if (motion_state_current != motion_old) {
            LOG_INF("Motion state changed - current: %d", motion_state_current);
            LOGIC_signal();
        }
    } else {
        /* zgodnie z poprzednią logiką – motion „wymuszone” */
        motion_state_current = 1;
    }

    /* Presence */
    if (settings.presence_enable) {
        bool present_old = present_state_current;
        present_state_current = presence_flag ? 1 : 0;

        if (present_state_current != present_old) {
            LOG_INF("Presence state changed - current: %d (motion=%d)",
                    present_state_current, motion_flag);
            LOGIC_signal();
        }
    } else {
        /* zgodnie z poprzednią logiką – presence „wymuszone” */
        present_state_current = 1;
    }
}

/* -------------------------------------------------------------------------- */
/* Inicjalizacja sensora                                                      */
/* -------------------------------------------------------------------------- */
int IR_SENSOR_init(void)
{
    uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
    uint8_t whoami = 0;
    sths34pf80_lpf_bandwidth_t lpf_m, lpf_p, lpf_p_m, lpf_a_t;
    int ret;

    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device is not ready");
        return 0;
    }

    ret = i2c_configure(i2c_dev, i2c_cfg);
    if (ret) {
        LOG_ERR("I2C config failed (%d)", ret);
        return 0;
    }

    ir_sensor.write_reg = platform_write;
    ir_sensor.read_reg  = platform_read;
    ir_sensor.mdelay    = platform_delay;

    /* Work do obsługi przerwania – ZANIM uzbroimy IRQ */
    k_work_init(&ir_int_work, ir_int_work_handler);

    /* GPIO INT */
    if (!gpio_is_ready_dt(&ir_sensor_int)) {
        LOG_ERR("INT device %s not ready", ir_sensor_int.port->name);
        return 0;
    }

    ret = gpio_pin_configure_dt(&ir_sensor_int, GPIO_INPUT);
    if (ret) {
        LOG_ERR("Failed to configure %s pin %d (%d)", ir_sensor_int.port->name, ir_sensor_int.pin, ret);
        return 0;
    }



    LOG_INF("INT on %s pin %d configured", ir_sensor_int.port->name, ir_sensor_int.pin);



    sths34pf80_boot_set(&ir_sensor, 1);
    k_msleep(1000);
    sths34pf80_boot_set(&ir_sensor, 0);
    /* Boot time */
    platform_delay(BOOT_TIME);

    /* WHOAMI */
    (void)sths34pf80_device_id_get(&ir_sensor, &whoami);
    if (whoami != STHS34PF80_ID) {
        LOG_ERR("DEVICE WRONG ID: 0x%02X", whoami);
        /* można tu zwrócić 0; zostawiamy łagodnie */
    } else {
        LOG_INF("DEVICE OK (ID=0x%02X)", whoami);
    }

    /* Averaging */
    (void)sths34pf80_avg_tobject_num_set(&ir_sensor, STHS34PF80_AVG_TMOS_32);
    (void)sths34pf80_avg_tambient_num_set(&ir_sensor, STHS34PF80_AVG_T_8);

    /* Motion LPF */
    (void)sths34pf80_lpf_m_bandwidth_set(&ir_sensor, STHS34PF80_LPF_ODR_DIV_800); // MOTION

    // sths34pf80_lpf_p_bandwidth_set(&ir_sensor, STHS34PF80_LPF_ODR_DIV_800);
    // sths34pf80_lpf_p_m_bandwidth_set(&ir_sensor, STHS34PF80_LPF_ODR_DIV_800);
    // sths34pf80_lpf_a_t_bandwidth_set(&ir_sensor, STHS34PF80_LPF_ODR_DIV_800);

    /* Read filters (diag) */
    (void)sths34pf80_lpf_m_bandwidth_get(&ir_sensor, &lpf_m);
    (void)sths34pf80_lpf_p_bandwidth_get(&ir_sensor, &lpf_p);
    (void)sths34pf80_lpf_p_m_bandwidth_get(&ir_sensor, &lpf_p_m);
    (void)sths34pf80_lpf_a_t_bandwidth_get(&ir_sensor, &lpf_a_t);
    LOG_INF("LPF m:%d p:%d p_m:%d a_t:%d", lpf_m, lpf_p, lpf_p_m, lpf_a_t);

    /* BDU */
    (void)sths34pf80_block_data_update_set(&ir_sensor, 1);

    /* Presence ABS value – wg aktualnej konfiguracji (0/1 w zależności od potrzeb) */
    (void)sths34pf80_presence_abs_value_set(&ir_sensor, 0);

    /* Threshold/hysteresis */
    (void)sths34pf80_presence_threshold_set(&ir_sensor, settings.presence_threshold);
    (void)sths34pf80_presence_hysteresis_set(&ir_sensor, 32); // 8 bit

    (void)sths34pf80_motion_threshold_set(&ir_sensor, settings.motion_threshold);
    (void)sths34pf80_motion_hysteresis_set(&ir_sensor, 100); // 8 bit

    /* INT source: motion + presence, OR routing */


    (void)sths34pf80_int_or_set(&ir_sensor, STHS34PF80_INT_MOTION_PRESENCE);
    (void)sths34pf80_route_int_set(&ir_sensor, STHS34PF80_INT_OR);

    /* ODR */
    (void)sths34pf80_odr_set(&ir_sensor, STHS34PF80_ODR_AT_30Hz);



    //     /* Rejestracja callbacku PRZED uzbrojeniem przerwania */
    // gpio_init_callback(&irsensor_int_cb_data, irsensor_int_cb, BIT(ir_sensor_int.pin));
    // gpio_add_callback(ir_sensor_int.port, &irsensor_int_cb_data);

    // /* Możesz rozważyć EDGE_FALLING jeśli pin jest ACTIVE_LOW w DT */
    // ret = gpio_pin_interrupt_configure_dt(&ir_sensor_int, GPIO_INT_EDGE_BOTH);
    // if (ret) {
    //     LOG_ERR("Failed to setup interrupt on %s pin %d (%d)", ir_sensor_int.port->name, ir_sensor_int.pin, ret);
    //     return 0;
    // }

    return 1;
}

/* -------------------------------------------------------------------------- */
/* API: runtime zmiana threshold                                              */
/* -------------------------------------------------------------------------- */
void IR_SENSOR_set_new_motion_threshold(uint16_t threshold)
{
    (void)sths34pf80_motion_threshold_set(&ir_sensor, threshold);
}

void IR_SENSOR_set_new_presence_threshold(uint16_t threshold)
{
    (void)sths34pf80_presence_threshold_set(&ir_sensor, threshold);
}

/* -------------------------------------------------------------------------- */
/* API: odczyt wszystkich surowych danych                                     */
/* -------------------------------------------------------------------------- */
int IR_SENSOR_get_all_raw_data(IR_SENSOR_raw_data_t* d)
{
    if (!d) return -EINVAL;

    (void)sths34pf80_tobject_raw_get(&ir_sensor, &d->object_raw);
    (void)sths34pf80_tambient_raw_get(&ir_sensor, &d->tambient_raw);
    (void)sths34pf80_tobj_comp_raw_get(&ir_sensor, &d->tobj_comp_raw);
    (void)sths34pf80_tpresence_raw_get(&ir_sensor, &d->tpresence_raw);
    (void)sths34pf80_tmotion_raw_get(&ir_sensor, &d->tmotion_raw);
    (void)sths34pf80_tamb_shock_raw_get(&ir_sensor, &d->tamb_shock_raw);
    (void)sths34pf80_presence_abs_value_get(&ir_sensor, &d->presence_abs_value);

    return 0; // Success
}

/* -------------------------------------------------------------------------- */
/* API: surowe kanały motion / presence                                       */
/* Uwaga: nazwa "montion" zachowana zgodnie z istniejącym wywołaniem w main.c */
/* -------------------------------------------------------------------------- */
int16_t IR_SENSOR_get_montion_raw(void)
{
    int16_t raw = 0;
    (void)sths34pf80_tmotion_raw_get(&ir_sensor, &raw);
    return raw;
}

int16_t IR_SENSOR_get_presence_raw(void)
{
    int16_t raw = 0;
    (void)sths34pf80_tpresence_raw_get(&ir_sensor, &raw);
    return raw;
}

/* -------------------------------------------------------------------------- */
/* API: reset algorytmu                                                       */
/* -------------------------------------------------------------------------- */
void IR_SENSOR_reset(void)
{
    (void)sths34pf80_algo_reset(&ir_sensor);
}

/* -------------------------------------------------------------------------- */
/* Platform I2C                                                               */
/* -------------------------------------------------------------------------- */
static int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len)
{
    ARG_UNUSED(handle);

    if (len > 99) {
        return -EINVAL; /* zabezpieczenie buffera [0]=Reg + 99 danych */
    }

    uint8_t buff[100];
    buff[0] = Reg;
    memcpy(&buff[1], Bufp, len);
    return i2c_write(i2c_dev, buff, 1 + len, IRSENSOR_I2C_ADDR);
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
