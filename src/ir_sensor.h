
#ifndef __IR_SENSOR_H
#define __IR_SENSOR_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>


#define    BOOT_TIME         10 //ms

int IR_SENSOR_init(void);


#endif
