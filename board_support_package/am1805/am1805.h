/**
 * @file am1805.h
 * @brief Professional driver for AM1805 RTC with Auto-Fallback and Dynamic Calibration.
 * @encoding UTF-8
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* --- Configuration Macros --- */
#define AM1805_ADDR             (0x69 << 1)
#define RTC_CAL_TEMP_THRESHOLD    2.0f    /* Temp delta to trigger calibration */
#define RTC_MIN_VALID_TEMP       -50.0f   /* Sensor sanity check */

/* --- Register Map --- */
#define AM1805_REG_HUNDREDTHS   0x00
#define AM1805_REG_STATUS       0x0F
#define AM1805_REG_CTRL1        0x10
#define AM1805_REG_CAL_XT       0x14  /* XT Calibration (1 step = 2.03 ppm) */
#define AM1805_REG_CAL_RC       0x15  /* RC Calibration (1 step = 1.72 ppm) */
#define AM1805_REG_OCTRL        0x1C  /* Oscillator Control */
#define AM1805_REG_OSC_STATUS   0x1D  /* Oscillator Status */
#define AM1805_REG_KEY          0x1F  /* Configuration Key */

typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR
} AM1805_Status;

typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month;
    uint16_t year;
    bool is_xt_active;  /* TRUE = Running on Crystal, FALSE = RC Fallback active */
} AM1805_Time_t;

/* --- Public API --- */

/**
 * @brief Initialize RTC with XT priority and Auto RC fallback (FOS=1).
 */
bool AM1805_Init(I2C_HandleTypeDef *hi2c_ptr);

/**
 * @brief Read current time and oscillator status.
 */
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);

/**
 * @brief Write time to RTC.
 */
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);

/**
 * @brief Apply calibration based on current active oscillator (XT parabolic or RC linear).
 */
AM1805_Status AM1805_AutoCalibrate(float temp);

/**
 * @brief Parse __DATE__ and __TIME__ macros for initial sync.
 */
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);

/**
 * @brief Enable 32.768 kHz output on Pin 8 (CLKOUT).
 */
AM1805_Status AM1805_Test_EnableClkOut(void);

void AM1805_FormatTime(char* buf, uint16_t len, AM1805_Time_t* time);

#endif /* AM1805_H */
