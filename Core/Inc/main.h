/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define W_UP_EXTI14_Pin GPIO_PIN_4
#define W_UP_EXTI14_GPIO_Port GPIOE
#define DONE_Pin GPIO_PIN_6
#define DONE_GPIO_Port GPIOF
#define RESET_XS6_Pin GPIO_PIN_7
#define RESET_XS6_GPIO_Port GPIOF
#define SEL_MEM_STM32_Pin GPIO_PIN_8
#define SEL_MEM_STM32_GPIO_Port GPIOF
#define SPI2_NSS_Pin GPIO_PIN_1
#define SPI2_NSS_GPIO_Port GPIOC
#define SCK_STM32_XC6_Pin GPIO_PIN_5
#define SCK_STM32_XC6_GPIO_Port GPIOA
#define MISO_XC6_STM32_Pin GPIO_PIN_6
#define MISO_XC6_STM32_GPIO_Port GPIOA
#define MOSI_STM32_XC6_Pin GPIO_PIN_7
#define MOSI_STM32_XC6_GPIO_Port GPIOA
#define NSS_STM32_XC6_Pin GPIO_PIN_4
#define NSS_STM32_XC6_GPIO_Port GPIOC
#define GPIO_0_Pin GPIO_PIN_0
#define GPIO_0_GPIO_Port GPIOB
#define GPIO_1_Pin GPIO_PIN_1
#define GPIO_1_GPIO_Port GPIOB
#define CAN2_TX_Pin GPIO_PIN_13
#define CAN2_TX_GPIO_Port GPIOB
#define TMR_DMA_LED_Pin GPIO_PIN_6
#define TMR_DMA_LED_GPIO_Port GPIOC
#define UART_TX_KPA_Pin GPIO_PIN_9
#define UART_TX_KPA_GPIO_Port GPIOA
#define UART_RX_KPA_Pin GPIO_PIN_10
#define UART_RX_KPA_GPIO_Port GPIOA
#define STLINK_GND_Pin GPIO_PIN_7
#define STLINK_GND_GPIO_Port GPIOB
#define TP0_Pin GPIO_PIN_0
#define TP0_GPIO_Port GPIOE
#define TP1_Pin GPIO_PIN_1
#define TP1_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
