#include <zephyr/kernel.h>
#include <nrfx_saadc.h>
#include "adc.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adc, CONFIG_LOG_DEFAULT_LEVEL);


static volatile bool is_ready;
static uint16_t samples[2];

static const nrfx_saadc_channel_t channel[2] = {	NRFX_SAADC_DEFAULT_CHANNEL_SE(NRF_SAADC_INPUT_VDD,  0),
									        		NRFX_SAADC_DEFAULT_CHANNEL_SE(NRF_SAADC_INPUT_AIN3, 1) };

static struct k_timer adc_timer;



static void adc_handler(struct k_work *work)
{
	uint16_t light = 0;

	light = get_light_intensity();

	LOG_INF("Light: %d", light);
}


void adc_init(void)
{
	nrfx_err_t err_code;

	err_code = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
	err_code = nrfx_saadc_channels_config(channel, 2);
	uint32_t channels_mask = nrfx_saadc_channels_configured_get();
	err_code = nrfx_saadc_simple_mode_set(channels_mask,
										  NRF_SAADC_RESOLUTION_12BIT,
										  NRF_SAADC_OVERSAMPLE_256X,
										  NULL);
	err_code = nrfx_saadc_buffer_set(samples, 2);

	k_timer_init(&adc_timer, adc_handler, NULL);
	k_timer_start(&adc_timer, K_MSEC(100), K_MSEC(100) );
}


uint16_t get_light_intensity(void)
{
    uint16_t light;
    nrfx_err_t err_code;

	err_code = nrfx_saadc_mode_trigger();
	light = samples[1];

    return light;
}
