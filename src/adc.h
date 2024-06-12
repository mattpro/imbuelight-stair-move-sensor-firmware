
#ifndef __ADC_H
#define __ADC_H

#include <zephyr/kernel.h>

void ADC_init(void);
int16_t ADC_get_light_intensity(void);


#endif

