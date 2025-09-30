#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>



#include "logic.h"
#include "settings.h"
#include "utils.h"

extern bool present_state_current;
extern bool motion_state_current;
extern bool present_state; /* nieużywane tutaj, ale istnieje w projekcie */
extern bool motion_state;  /* nieużywane tutaj, ale istnieje w projekcie */



#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(logic, CONFIG_LOG_DEFAULT_LEVEL);

// This fuction is called every time if sensor state is changed
void LOGIC_signal(void)
{
	static bool new_sensor_state;

	// light_state_current - current light sensor state
	// present_state_current - current presetn sensor state

	// TODO: gdzie odwrócić sygnały sensor state i light state?

	// Light state logic invert
	if ( settings.light_intensity_out_invert == false )
	{
		light_state = light_state_current; 
	}
	else
	{
		light_state = !light_state_current;
	}

	// Light state logic invert
	if ( settings.presence_out_invert == false )
	{
		present_state = present_state_current; 
	}
	else
	{
		present_state = !present_state_current;
	}

	if ( settings.motion_out_invert == false )
	{
		motion_state = motion_state_current; 
	}
	else
	{
		motion_state = !motion_state_current;
	}



	// BOTH SENSOR ENABLED
	if ( ( settings.presence_enable == true ) && ( settings.light_intensity_enable == true ) )
	{
		if ( settings.signal_out_logic_function == OUT_LOGIC_AND )
		{
			new_sensor_state = light_state & present_state;
		}
		else if ( settings.signal_out_logic_function == OUT_LOGIC_OR )
		{
			new_sensor_state = light_state | present_state;
		}
		else
		{
			new_sensor_state = 0;
		}
	}
	// ONLY PRESENSC SENSOR ENABLED
	else if ( ( settings.presence_enable == true ) && ( settings.light_intensity_enable == false ) )
	{
		if ( settings.signal_out_logic_function != OUT_LOGIC_NONE )
		{
			new_sensor_state = present_state;
		}
	}
	// ONLY LIGHT SENSOR ENABLED
	else if ( ( settings.presence_enable == false ) && ( settings.light_intensity_enable == true ) )
	{
		if ( settings.signal_out_logic_function != OUT_LOGIC_NONE )
		{
			new_sensor_state = light_state;
		}
	}

	// Out signal logic
	if ( settings.signal_out_invert == false )
	{
		sensor_state = new_sensor_state;
	}
	else
	{
		sensor_state = !new_sensor_state;
	}
 	OUT_set(sensor_state); 
 

	// LED State controll
	switch(settings.led_signalization_src)
	{
		case LED_SIGNALIZATION_SRC_MOVE_SENSOR:
			led_state = motion_state;
		break;
		case LED_SIGNALIZATION_SRC_LIGHT_SENSOR:
			led_state = light_state;
		break;
		case LED_SIGNALIZATION_SRC_OUT_SIGNAL:
			led_state = sensor_state;
		break;
		default:
			led_state = false;
		break;	
	}
	LED_set(led_state);

}