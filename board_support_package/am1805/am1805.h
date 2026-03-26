/**
 * @file am1805.h
 * @brief Потокобезопасный драйвер для RTC AM1805AQ.
 * Оптимизировано для: STM32F405, CMSIS_OS_V2 (FreeRTOS).
 * Характеристики: VCC 3.3V, VBAT 2.89V, Кварц 12.5pF (FC-12M).
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

/* --- Конфигурация шины --- */
#define AM1805_ADDR              (0x69 << 1) // 7-битный адрес 0x69, сдвинутый для HAL
#define AM1805_I2C_TIMEOUT       200         // Таймаут HAL I2C в мс
#define AM1805_MUTEX_TIMEOUT     osWaitForever

/* --- Карта регистров (согласно Datasheet v2.0) --- */
#define AM1805_REG_HUNDREDTHS    0x00 // Сотые доли секунды (запись сюда сбрасывает предделители)
#define AM1805_REG_SECONDS      0x01 // Секунды
#define AM1805_REG_STATUS        0x0F // Статус системы (флаги прерываний, BAT и др.)
#define AM1805_REG_CTRL1        0x10 // Управление 1 (биты STOP, WRTC, PWR2)
#define AM1805_REG_SQW           0x13 // Управление выходом квадратной волны / CLKOUT
#define AM1805_REG_CAL_XT       0x14 // Цифровая калибровка кварца (OFFSETX)
#define AM1805_REG_OSC_CTRL      0x1C // Управление осциллятором (OSEL, PWGT, OUT)
#define AM1805_REG_OSC_STAT      0x1D // Статус осциллятора (флаги OF, ACF, бит OMODE)
#define AM1805_REG_KEY           0x1F // Регистр для записи ключей разблокировки
#define AM1805_REG_BREF          0x21 // Порог переключения питания (Battery Reference)
#define AM1805_REG_BATMODE_IO    0x27 // Управление изоляцией I/O шины (бит IOBM)

/* --- Ключи разблокировки доступа (стр. 97) --- */
/* После любой записи в защищенный регистр ключ сбрасывается в 0, его нужно писать снова */
#define AM1805_KEY_OSC           0xA1 // Ключ для доступа к регистру OSC_CTRL (0x1C)
#define AM1805_KEY_CONF          0x9D // Ключ для BREF (0x21), CAL (0x14) и BATMODE (0x27)

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
    uint16_t year;       // Формат: 2024
    bool is_xt_active;   // true, если чип работает от кварца (не от RC)
} AM1805_Time_t;

/**
 * @brief Структура для чтения отладочной информации
 */
typedef struct {
    uint8_t cal_xt, osc_ctrl, bref, status, ctrl1, osc_stat;
} AM1805_Diag_t;

/* --- Публичное API --- */

// Инициализация аппаратной части (Кварц, Питание, Изоляция шины)
bool          AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);

// Работа со временем
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);

// Настройка точности (программная калибровка)
AM1805_Status AM1805_SetCalibration(float adj_ppm);


// Диагностика и форматирование
AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag);
void          AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);

#endif /* AM1805_H */
