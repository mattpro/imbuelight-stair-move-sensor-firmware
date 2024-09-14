
#ifndef __IR_SENSOR_H
#define __IR_SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>


#define    BOOT_TIME         10 //ms

typedef struct {
	int16_t object_raw;
    int16_t tambient_raw;
    int16_t tobj_comp_raw;
    int16_t tpresence_raw;
    int16_t tmotion_raw;
    int16_t tamb_shock_raw;
    uint8_t presence_abs_value;
} IR_SENSOR_raw_data_t;

extern IR_SENSOR_raw_data_t IR_SENSOR_raw_data;


int IR_SENSOR_init(void);
void IR_SENSOR_set_new_threshold(uint16_t threshold);
int16_t IR_SENSOR_get_raw(void);
void IR_SENSOR_reset(void);
int IR_SENSOR_get_all_raw_data(IR_SENSOR_raw_data_t* IR_SENSOR_raw_data);

#endif
