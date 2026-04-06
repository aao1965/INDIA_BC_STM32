/**
 * @file am1805.h
 * @brief Потокобезопасный драйвер для RTC AM1805AQ.
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/* --- Параметры I2C --- */
#define AM1805_ADDR              (0x69 << 1)
#define AM1805_I2C_TIMEOUT       100
#define AM1805_MUTEX_TIMEOUT     osWaitForever

/* --- Адреса регистров --- */
#define AM1805_REG_HUNDREDTHS    0x00
#define AM1805_REG_SECONDS       0x01
#define AM1805_REG_MINUTES       0x02
#define AM1805_REG_HOURS         0x03
#define AM1805_REG_DATE          0x04
#define AM1805_REG_MONTH         0x05
#define AM1805_REG_YEAR          0x06
#define AM1805_REG_WEEKDAY       0x07

#define AM1805_REG_STATUS        0x0F
#define AM1805_REG_CTRL1         0x10
#define AM1805_REG_CAL_XT        0x14
#define AM1805_REG_OSC_CTRL      0x1C
#define AM1805_REG_OSC_STAT      0x1D
#define AM1805_REG_KEY           0x1F
#define AM1805_REG_BREF          0x21
#define AM1805_REG_BATMODE_IO    0x27

/* --- Командные ключи и маски --- */
#define AM1805_KEY_OSC           0xA1
#define AM1805_KEY_CONF          0x9D
#define AM1805_KEY_RESET         0x3C
#define AM1805_CTRL1_24H         0x40

typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR,
    AM1805_BUSY
} AM1805_Status;

typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month, weekday;
    uint16_t year;
    bool is_xt_active;
} AM1805_Time_t;

typedef struct {
    uint8_t status;      // 0x0F
    uint8_t ctrl1;       // 0x10
    uint8_t cal_xt;      // 0x14
    uint8_t osc_ctrl;    // 0x1C
    uint8_t osc_stat;    // 0x1D
    uint8_t bref;        // 0x21
    uint8_t batmode_io;  // 0x27
} AM1805_Diag_t;

/* --- Основной интерфейс --- */
bool          AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);
AM1805_Status AM1805_SoftwareReset(void);
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour);
AM1805_Status AM1805_GetDiagnostics(AM1805_Diag_t *diag);
void          AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);
bool          AM1805_ForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password);
bool          AM1805_ResetAndForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password);

#endif /* AM1805_H */
