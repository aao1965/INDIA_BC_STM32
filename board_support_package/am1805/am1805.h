/**
 * @file am1805.h
 * @brief Thread-safe driver for AM1805 RTC with Mutex support (CMSIS-OS2).
 * @encoding UTF-8
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/* --- Configuration --- */
#define AM1805_ADDR              (0x69 << 1)
#define AM1805_I2C_TIMEOUT       200      /* HAL I2C timeout in ms */
#define AM1805_MUTEX_TIMEOUT     300      /* Mutex wait timeout in ms */

/* --- Debug Options --- */
#define AM1805_ENABLE_CLKOUT                /* Uncomment to enable Pin 8 output in Init */

#ifdef AM1805_ENABLE_CLKOUT
  #define AM1805_CLKOUT_FREQ    0x01        /* 0x01 = 32.768 kHz (XT Direct) */
  /* #define AM1805_CLKOUT_FREQ  0x10 */    /* 0x10 = 1 Hz */
#endif

/* --- Calibration Settings --- */
#define RTC_CAL_TEMP_THRESHOLD    5.0f    
#define RTC_MIN_VALID_TEMP       -50.0f   

/* --- Register Map --- */
#define AM1805_REG_HUNDREDTHS    0x00
#define AM1805_REG_STATUS        0x0F
#define AM1805_REG_CTRL1         0x10
#define AM1805_REG_CTRL2         0x11
#define AM1805_REG_SQW           0x13
#define AM1805_REG_CAL_XT        0x14
#define AM1805_REG_OCTRL         0x1C
#define AM1805_REG_OSTAT         0x1D
#define AM1805_REG_KEY           0x1F

/* --- Access Keys --- */
#define AM1805_KEY_OSC           0xA1  /* For 0x10-0x1E registers */
#define AM1805_KEY_EXT           0x9D  /* For 0x20-0x30 and time write */

typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR,
    AM1805_BUSY
} AM1805_Status;

typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month;
    uint16_t year;
    bool is_xt_active;  /* TRUE = Crystal, FALSE = RC Fallback */
} AM1805_Time_t;

/* --- Public API --- */
bool AM1805_Init(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);
AM1805_Status AM1805_AutoCalibrate(float temp);
void AM1805_FormatTime(char* buf, uint16_t len, AM1805_Time_t* time);
void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);

#endif /* AM1805_H */
