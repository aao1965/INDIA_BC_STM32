/*
 * board_support_package.c
 *
 *  Created on: Mar 18, 2026
 *      Author: aao19
 */


#include "board_support_package.h"
#include "pin_mgmt.h"
#include "rgb_led.h"
#include "ds1621.h"
#include "am1805.h"
#include "terminal.h"
#include "terminal_signals.h"

// Global hardware test result variable
uint32_t test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;


/* LED handle */
LED_Handler_t led_main;

// Initialize all hardware components
uint32_t init_hardware(void) {

	// Initialize GPIO
	if (PIN_Mgmt_Init()!=osOK){
		test_hardware_result |= _B_FAULT_GPIO_;
	}else{
		SPI_Bus_Release_To_FPGA();
		if (FPGA_System_Restart(4000)!= osOK){
			PIN_Reset(&pin_reset_xs6);
			test_hardware_result |= _B_FAULT_FPGA_;
		}
	}

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


	 // TERMINAL (eAssist) INITIALIZATION
	if (!terminal_init()) {
		test_hardware_result |= _B_FAULT_TERMINAL_;
	}

	return test_hardware_result;
}


// Test if specific hardware module is functional
inline bool test_status_hardware(uint32_t module) {
    return !(get_status_hardware() & module);
}

// Get current hardware status
inline uint32_t get_status_hardware(void) {
    return test_hardware_result;
}

// Check if software reset occurred (call before HAL_Init)
inline bool get_rcc_csr(void) {
    return __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST);
}

// Perform system reset
void bsp_system_reset(void) {
    HAL_NVIC_SystemReset();
}



