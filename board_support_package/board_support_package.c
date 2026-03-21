/*
 * board_support_package.c
 *
 *  Created on: Mar 18, 2026
 *      Author: aao19
 */


#include "board_support_package.h"
#include "rgb_led.h"
#include "ds1621.h"
#include "am1805.h"

// Global hardware test result variable
uint32_t test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;


/* LED handle */
LED_Handler_t led_main;

// Initialize all hardware components
uint32_t init_hardware(void) {

	// Initialize RGB LED
	if (RGB_LED_Init(&led_main, &htim3, TIM_CHANNEL_1) != true) {
		test_hardware_result |= _B_FAULT_RGB_;
	}
	// Initialize ds1621 temperature sensot
	if (DS1621_Init(&hi2c1))
		if (DS1621_CreateTask() == NULL) {
			test_hardware_result |= _B_FAULT_DS1621_;
		}
	/* --- Inside init_hardware() --- */

	// 1. Initialize RTC Hardware
	if (AM1805_Init(&hi2c1) == false) {
		test_hardware_result |= _B_FAULT_AM1805_;
	} else {
		AM1805_Time_t now;

		// 2. GetTime now automatically fills now.is_xt_active
		if (AM1805_GetTime(&now) == AM1805_OK) {
			// If RTC was reset (battery failure), sync with build time
			if (now.year <= 2000) {
				AM1805_Time_t build = AM1805_ParseBuildTime(__DATE__, __TIME__);
				AM1805_SetTime(&build);
			}
		}

		// 3. Enable 32kHz output for hardware verification (Pin 8)
		AM1805_Test_EnableClkOut();
	}

	return test_hardware_result;
}


/*
int _write(int file, char *ptr, int len) {
    int i;
    for (i = 0; i < len; i++) {
        ITM_SendChar((*ptr++));
    }
    return len;
}
*/
