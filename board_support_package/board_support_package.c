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

	// 1. Initialize DS1621 first
		if (DS1621_Init(&hi2c1)) {
			if (DS1621_CreateTask() == NULL) {
				test_hardware_result |= _B_FAULT_DS1621_;
			}

			osMutexId_t shared_i2c_mutex = DS1621_GetI2CMutex();

			// 2. AM1805_Init_Smart enables XT and CLKOUT if defined
			if (AM1805_Init_Smart(&hi2c1, shared_i2c_mutex)) {

				// --- Diagnostic Block ---
				AM1805_Diag_t rtc_diag;
				if (AM1805_ReadDiagnostic(&rtc_diag) == AM1805_OK) {
					// Place breakpoint here to inspect rtc_diag in Watch window
					// Check rtc_diag.ostat: 0x00 means XT is active, 0x10 means RC fallback
					(void)rtc_diag;
				}
				// ------------------------

				AM1805_Time_t now;

				// 3. Read time to check if RTC is valid
				if (AM1805_GetTime(&now) == AM1805_OK) {
					if (now.year <= 2000) {
						AM1805_Time_t build = AM1805_ParseBuildTime(__DATE__, __TIME__);
						AM1805_SetTime(&build);
					}
				}

			} else {
				test_hardware_result |= _B_FAULT_AM1805_;
			}
		} else {
			test_hardware_result |= _B_FAULT_DS1621_;
			// Try to init RTC anyway (CLKOUT will still start if defined)
			AM1805_Init_Smart(&hi2c1, NULL);
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



