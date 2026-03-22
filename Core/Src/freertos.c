/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "board_support_package.h"
#include "pin_mgmt.h"
#include "rgb_led.h"
#include "ds1621.h"
#include "am1805.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern LED_Handler_t led_main;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LowLevelTask */
osThreadId_t LowLevelTaskHandle;
const osThreadAttr_t LowLevelTask_attributes = {
  .name = "LowLevelTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for TerminalTask */
osThreadId_t TerminalTaskHandle;
const osThreadAttr_t TerminalTask_attributes = {
  .name = "TerminalTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow7,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartLowLevelTask(void *argument);
void StartTerminalTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LowLevelTask */
  LowLevelTaskHandle = osThreadNew(StartLowLevelTask, NULL, &LowLevelTask_attributes);

  /* creation of TerminalTask */
  TerminalTaskHandle = osThreadNew(StartTerminalTask, NULL, &TerminalTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */

AM1805_Time_t current_time;
char time_str[32];          // Буфер для строки "HH:MM:SS [XT]"

/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

	osThreadSuspend(LowLevelTaskHandle);
	osThreadSuspend(TerminalTaskHandle);

    init_hardware();


    osThreadResume(LowLevelTaskHandle);
    osThreadResume(TerminalTaskHandle);

    for (;;) {

		if (AM1805_GetTime(&current_time) == AM1805_OK) {
			AM1805_FormatTime(time_str, sizeof(time_str), &current_time);
		}

		PIN_Toggle_S(&pin_tp0);
        osDelay(100);
    }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartLowLevelTask */
float current_temp;
uint8_t rtc_is_crystal;
/* USER CODE END Header_StartLowLevelTask */
void StartLowLevelTask(void *argument)
{
  /* USER CODE BEGIN StartLowLevelTask */
  float last_cal_temp = -999.0f;
  AM1805_Time_t rtc_status;

  for (;;) {
    // 1. Get current data
    float t = DS1621_GetLastTemperature();
    current_temp= t;

    AM1805_GetTime(&rtc_status); // Updates time and rtc_status.is_xt_active

    // 2. Dynamic Autocalibration (XT parabolic or RC linear)
    if (t > RTC_MIN_VALID_TEMP && fabsf(t - last_cal_temp) > RTC_CAL_TEMP_THRESHOLD) {
        if (AM1805_AutoCalibrate(t) == AM1805_OK) {
            last_cal_temp = t;
        }
    }

    // 3. LED Color Logic
    LED_Color_t color;
    float tc = (t < 20.0f) ? 20.0f : (t > 50.0f ? 50.0f : t);

    if (tc < 35.0f) {
        float ratio = (tc - 20.0f) / 15.0f;
        color.r = 0;
        color.g = (uint8_t)(40 * ratio);
        color.b = (uint8_t)(40 * (1.0f - ratio));
    } else {
        float ratio = (tc - 35.0f) / 15.0f;
        color.r = (uint8_t)(40 * ratio);
        color.g = (uint8_t)(40 * (1.0f - ratio));
        color.b = 0;
    }
    color.w = 0;

    // OPTIONAL: If running on RC (Crystal failed), add a Red blink or tint
    if (!rtc_status.is_xt_active) {
        color.r = 50; // Visual warning: Oscillator fallback active
    }

    RGB_LED_SetColor(&led_main, color);


    osDelay(500);

  }
  /* USER CODE END StartLowLevelTask */
}

/* USER CODE BEGIN Header_StartTerminalTask */
/**
* @brief Function implementing the TerminalTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTerminalTask */
void StartTerminalTask(void *argument) {
	/* USER CODE BEGIN StartTerminalTask */
	/* Infinite loop */
	for (;;) {

		PIN_Toggle_S(&pin_tp1);
		osDelay(10);
	}
	/* USER CODE END StartTerminalTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

