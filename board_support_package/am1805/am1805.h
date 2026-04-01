/**
 * @file am1805.h
 * @brief Потокобезопасный драйвер для RTC AM1805AQ.
 * Оптимизировано для: STM32F4 + FreeRTOS.
 * Адаптировано под кварц FC-12M (12.5pF) и батарею 2.85V.
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/* --- Конфигурация шины --- */
#define AM1805_ADDR              (0x69 << 1) // 7-битный адрес 0x69
#define AM1805_I2C_TIMEOUT       100         // Таймаут HAL I2C в мс
#define AM1805_MUTEX_TIMEOUT     osWaitForever

/* --- Карта регистров --- */
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

/* --- Ключи доступа --- */
#define AM1805_KEY_OSC           0xA1 // Для регистра OSC_CTRL (0x1C)
#define AM1805_KEY_CONF          0x9D // Для регистров BREF, CAL, BATMODE

/**
 * @brief Статусы выполнения функций драйвера
 */
typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR,
    AM1805_BUSY
} AM1805_Status;

/**
 * @brief Структура для хранения даты и времени
 */
typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month, weekday;
    uint16_t year;       // Формат: 2026
    bool is_xt_active;   // true, если работает от кварца
} AM1805_Time_t;

/**
 * @brief Структура для чтения отладочной информации
 */
typedef struct {
    uint8_t cal_xt, osc_ctrl, bref, status, ctrl1, osc_stat;
} AM1805_Diag_t;


/* ========================================================================== */
/* ИНТЕРФЕЙС                                   */
/* ========================================================================== */

// Инициализация аппаратной части (Кварц, Питание, Изоляция шины)
bool          AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);

// Работа со временем
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);

// Настройка точности (программная калибровка)
AM1805_Status AM1805_SetCalibration(float adj_ppm);

/**
 * @brief Настройка калибровки по реальному уходу времени.
 * @param drift_sec_per_hour Уход часов в секундах за 1 час.
 * Положительное число (+): часы спешат (убежали вперед).
 * Отрицательное число (-): часы отстают.
 */
AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour);

// Диагностика и форматирование
AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag);
void          AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);

#endif // AM1805_H