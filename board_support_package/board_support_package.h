/*
 * board_support_package.h
 *
 *  Created on: Mar 18, 2026
 *      Author: aao19
 */

#ifndef BOARD_SUPPORT_PACKAGE_H_
#define BOARD_SUPPORT_PACKAGE_H_

#include	<math.h>
#include	<stdlib.h>
#include	<stdbool.h>
#include	<stddef.h>
#include 	<string.h>
#include 	<stdio.h>
#include 	<stdint.h>
#include 	"main.h"
#include 	"cmsis_os2.h"
#include 	"cmsis_os.h"
#include 	"limits.h"
#include 	"float.h"

#include	"usefull_define.h"

/**********************************************************
 * 	STM32 hardware modules
 */
/* RGB led pwm timer */
extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_tim3_ch1_trig;

//  I2C handle ds1621s+ and AM1805 rtc
extern I2C_HandleTypeDef hi2c1;

// eAssist UART terminal
extern	UART_HandleTypeDef 	huart1;
extern	DMA_HandleTypeDef 	hdma_usart1_rx;
extern	DMA_HandleTypeDef 	hdma_usart1_tx;
extern	SPI_HandleTypeDef 	hspi1;

/* ********************************************************
 * test	fault bits  */
#define		_B_TEST_HARDWARE_SUCCESS_	0
#define		_B_FAULT_OS_				B0
#define		_B_FAULT_RGB_				B1
#define		_B_FAULT_DS1621_			B2
#define		_B_FAULT_GPIO_				B3
#define		_B_FAULT_FPGA_				B4
#define		_B_FAULT_AM1805_			B5
#define		_B_FAULT_W25Q16_			B6
#define		_B_FAULT_FSMC_				B7
#define		_B_FAULT_FRAM_				B8
#define		_B_FAULT_TERMINAL_			B15



/*	global functions */
uint32_t 	init_hardware(void);
bool 		test_status_hardware(uint32_t module);
uint32_t 	get_status_hardware(void);

bool		get_rcc_csr(void);
void		bsp_system_reset(void);

#endif /* BOARD_SUPPORT_PACKAGE_H_ */
