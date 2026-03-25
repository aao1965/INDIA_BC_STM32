/**
 * @file am1805.h
 * @brief Потокобезопасный драйвер для RTC AM1805 (VCC 3.3V, VBAT 2.89V).
 * Кодировка: UTF-8.
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/* --- Конфигурация --- */
#define AM1805_ADDR              (0x69 << 1)
#define AM1805_I2C_TIMEOUT       200
#define AM1805_MUTEX_TIMEOUT     osWaitForever

/* --- Карта регистров (Datasheet Table 6-1, 6-2) --- */
#define AM1805_REG_HUNDREDTHS    0x00
#define AM1805_REG_SECONDS      0x01
#define AM1805_REG_STATUS        0x0F
#define AM1805_REG_CTRL1        0x10
#define AM1805_REG_CAL_XT       0x14
#define AM1805_REG_OSC_CTRL      0x1C
#define AM1805_REG_OSC_STAT      0x1D
#define AM1805_REG_KEY           0x1F
#define AM1805_REG_BREF          0x21
#define AM1805_REG_BATMODE_IO    0x27

/* --- Ключи доступа (стр. 97) --- */
#define AM1805_KEY_OSC           0xA1  /* Для регистра OSC_CTRL (0x1C) */
#define AM1805_KEY_CONF          0x9D  /* Для BREF(0x21), CAL(0x14), BATMODE(0x27) */

typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR,
    AM1805_BUSY
} AM1805_Status;

typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month, weekday;
    uint16_t year;
    bool is_xt_active; /* true = работает от кварца, false = от RC */
} AM1805_Time_t;

typedef struct {
    uint8_t cal_xt, osc_ctrl, bref, status, ctrl1;
} AM1805_Diag_t;

/* --- API функции --- */
bool          AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetCalibration(float adj_ppm);
AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag);
void          AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);

#endif /* AM1805_H */