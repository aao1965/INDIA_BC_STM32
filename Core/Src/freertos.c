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
#include "terminal.h"

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



/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

	osThreadSuspend(LowLevelTaskHandle);
	osThreadSuspend(TerminalTaskHandle);

    init_hardware();

	if (test_status_hardware(_B_FAULT_TERMINAL_)) {
		osThreadResume(TerminalTaskHandle);
	}

    osThreadResume(LowLevelTaskHandle);

    for (;;) {



		PIN_Toggle_S(&pin_tp0);
        osDelay(100);
    }
  /* USER CODE END StartDefaultTask */
}


/* USER CODE BEGIN Header_StartLowLevelTask */
float current_temp;
AM1805_Time_t current_time;
char time_str[32];          // Буфер для строки "HH:MM:SS [XT]"
/* USER CODE END Header_StartLowLevelTask */

void StartLowLevelTask(void *argument) {
    /* USER CODE BEGIN StartLowLevelTask */
    float t;
    float last_cal_temp = -999.0f;
    uint8_t sensor_retry_count = 0;
    const uint8_t MAX_SENSOR_RETRIES = 5;

    // 1. SMART WAIT for the first valid sensor reading
    // Try to get real temperature, but don't hang forever if the sensor is dead.
    while (sensor_retry_count < MAX_SENSOR_RETRIES) {
        last_cal_temp = DS1621_GetLastTemperature();

        if (last_cal_temp > RTC_MIN_VALID_TEMP) {
            break; // Sensor is alive and healthy!
        }

        sensor_retry_count++;
        osDelay(200); // Give it some time to wake up
    }

    // 2. FALLBACK logic if sensor is dead
    if (sensor_retry_count >= MAX_SENSOR_RETRIES) {
        last_cal_temp = 25.0f; // Use default room temp if sensor failed
        // Optional: Log error or set a "Sensor Error" flag for the UI
    }

    // 3. Initial calibration
    AM1805_AutoCalibrate(last_cal_temp);

    for (;;) {
        // 4. Update current temperature
        t = DS1621_GetLastTemperature();

        // Update global variable only if reading is valid
        if (t > RTC_MIN_VALID_TEMP) {
            current_temp = t;

            // 5. Dynamic Autocalibration logic
            if (fabsf(t - last_cal_temp) > RTC_CAL_TEMP_THRESHOLD) {
                if (AM1805_AutoCalibrate(t) == AM1805_OK) {
                    last_cal_temp = t;
                }
            }
        }

        // 6. Update Time and Format string
        if (AM1805_GetTime(&current_time) == AM1805_OK) {
            AM1805_FormatFullDateTime(time_str, sizeof(time_str), &current_time);
        }

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
		terminal_task();
		PIN_Toggle_S(&pin_tp1);

	}
	/* USER CODE END StartTerminalTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

