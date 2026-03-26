
#ifndef TERMINAL_SIGNALS_H_
#define TERMINAL_SIGNALS_H_

#include "board_support_package.h"
#include "rgb_led.h"
#include "am1805.h"

#define		DSPA_SIGNALS_NAME		dspa


//	test hardware
extern	uint32_t	test_hardware_result;
extern 	char time_str[];
extern 	AM1805_Diag_t rtc_diag;

// RGB leds
extern LED_Handler_t led_main;

// debug section
extern float current_temp;

int	init_terminal_signals(void);

#endif /* TERMINAL_SIGNALS_H_ */
