
#ifndef __ADC_H
#define __ADC_H

#include <zephyr/kernel.h>

void adc_init(void);
uint16_t get_light_intensity(void);


#endif

