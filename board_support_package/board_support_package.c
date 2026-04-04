/*
 * board_support_package.c
 *
 *  Created on: Mar 18, 2026
 *      Author: aao19
 */


#include <w25q16/w25q16.h>
#include "board_support_package.h"
#include "pin_mgmt.h"
#include "rgb_led.h"
#include "led_blink.h"
#include "ds1621.h"
#include "am1805.h"
#include "fsmc_memory.h"
#include "fm22l16.h"
#include "fpga.h"
#include "fpga_pwm.h"
#include "fpga_ic.h"
#include "spi_fpga_bridge.h"

#include "terminal.h"
#include "terminal_signals.h"

// Global hardware test result variable
uint32_t test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;


/* LED handle */
LED_Handler_t led_main;
W25Q16_Handle_t DD15;	// FPGA external flash memory handle
AM1805_Diag_t rtc_diag;
bool	soft_reset;

// Initialize all hardware components
uint32_t init_hardware(void) {

	DWT_Init();
	soft_reset=	get_rcc_csr();

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

	// W25Q16 FLASH CONFIGURATION
	// Configure after pins are initialized
	if (test_status_hardware(_B_FAULT_GPIO_)) {
		if (W25Q16_Init_With_PinMgmt(&DD15, &hspi1) != osOK) {
			test_hardware_result |= _B_FAULT_W25Q16_;
		} else {
			SPI_Bus_Acquire_For_STM32();
			// Read flash identification
			W25Q16_ReadJEDECID(&DD15);
			W25Q16_ReadUID(&DD15);
			// Release SPI bus and FPGA reset
			SPI_Bus_Release_To_FPGA();

		}
	}


	// Initialize STM32 <-> fpga bridge SPI
	if (!SPI_FPGA_Init(&hspi2)){
		test_hardware_result |= _B_FAULT_SPI_FPGA_;
	}else{

	}

	// Initialize fsmc library and recursive mutex
	if (fsmc_init() != FSMC_OK) {
		test_hardware_result |= _B_FAULT_FSMC_;
	} else {
		// FSMC bus is ready for use
		if (fm22_test_quick() != FSMC_OK) {
			test_hardware_result |= _B_FAULT_FRAM_;
		}

		// Проверяем связь с ПЛИС (чтение константы)
		if (fpga_test_link() != FSMC_OK) {
			test_hardware_result |= _B_FAULT_EMI_FPGA_;
		} else {
			// ПЛИС на связи, настраиваем периферию
			fpga_pwm_init(10000, 5000);

			// Инициализируем контроллер прерываний и задачи FreeRTOS
			if (setup_fpga_interrupts() != pdPASS) {
				// Сюда попадем, если не хватило RAM для задачи/семафора
				// или пропала связь по FSMC в процессе настройки
				test_hardware_result |= _B_FAULT_EMI_FPGA_; // или _B_FAULT_OS_ERROR_
			}
		}
	}

	// Initialize RGB LED
	if (RGB_LED_Init(&led_main, &htim3, TIM_CHANNEL_1) != true) {
		test_hardware_result |= _B_FAULT_RGB_;
	}else{
		led_blink_init(&led_main);
	}

	// 1. Сначала инициализируем DS1621
	if (DS1621_Init(&hi2c1)) {
		if (DS1621_CreateTask() == NULL) {
			test_hardware_result |= _B_FAULT_DS1621_;
		}

		osMutexId_t shared_i2c_mutex = DS1621_GetI2CMutex();

		// 2. Умная инициализация RTC (XT, BREF, IOBM)
		if (AM1805_Init_Smart(&hi2c1, shared_i2c_mutex)) {
	//	if 	(AM1805_ForceInit(&hi2c1, shared_i2c_mutex, 0x55AA3C0F)){
			// --- Блок диагностики ---
			// Используем согласованное имя функции
			if (AM1805_GetDiagnostics(&rtc_diag) == AM1805_OK) {
				// В Watch window смотрим rtc_diag.osc_stat:
				// 0x00 - XT активен, 0x10 - переход на RC (ошибка)
				(void) rtc_diag;
			}
			// ------------------------

			AM1805_Time_t now;

			// 3. Читаем время. Если оно сброшено (год <= 2000), ставим дату сборки
			if (AM1805_GetTime(&now) == AM1805_OK) {
				if (now.year <= 2000) {
					AM1805_Time_t build = AM1805_ParseBuildTime(__DATE__,	__TIME__);
					AM1805_SetTime(&build);
				}
			}

		} else {
			test_hardware_result |= _B_FAULT_AM1805_;
		}
	} else {
		test_hardware_result |= _B_FAULT_DS1621_;
		// RTC не инициализируем согласно твоей логике (не нужен без DS1621)
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


// Perform system reset
void bsp_system_reset(void) {
    HAL_NVIC_SystemReset();
}

// check reset
inline bool get_rcc_csr(void) {
	// Проверяем флаг (макрос возвращает SET или RESET)
	bool is_soft_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET);

	// ОЧИЩАЕМ все флаги причин сброса!
	// Это гарантирует, что следующая перезагрузка покажет только свежую причину.
	__HAL_RCC_CLEAR_RESET_FLAGS();

	return is_soft_reset;
}

/********************************* Точная us задержка *********************************************/
// Инициализация DWT
void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // Разблокировка доступа к DWT (актуально для старших ядер Cortex-M)
    // Разкомментируйте, если компилятор знает регистр LAR:
    // DWT->LAR = 0xC5ACCE55;

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// Деинициализация DWT
void DWT_DeInit(void) {
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

// Микросекундная блокирующая задержка
void delay_us(uint32_t us) {
    // Вычисляем, сколько тактов процессора нужно прождать
    uint32_t ticks_needed = us * (SystemCoreClock / 1000000U);

    // Фиксируем стартовое значение счетчика
    uint32_t tick_start = DWT->CYCCNT;

    // Ждем, пока разница не достигнет нужного количества тактов.
    // Переполнение uint32_t обрабатывается аппаратно и безопасно!
    while ((DWT->CYCCNT - tick_start) < ticks_needed) {
        __NOP(); // Опционально: пустая инструкция, чтобы не сводить с ума отладчик
    }
}


/********************************** WEAK INTERRUPT CALLBACK ***************************************/


