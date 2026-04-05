/**
 * @file am1805.h
 * @brief Потокобезопасный драйвер для RTC AM1805AQ.
 * Учтены особенности инициализации после замены кварца или сброса питания.
 */

#ifndef AM1805_H
#define AM1805_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

/* --- Параметры I2C --- */
#define AM1805_ADDR              (0x69 << 1) // 7-битный адрес 0x69, сдвинутый для HAL
#define AM1805_I2C_TIMEOUT       100         // Таймаут в мс
#define AM1805_MUTEX_TIMEOUT     osWaitForever

/* --- Адреса регистров (согласно Datasheet v2.0) --- */
#define AM1805_REG_HUNDREDTHS    0x00 // Сотые доли секунды
#define AM1805_REG_SECONDS       0x01
#define AM1805_REG_MINUTES       0x02
#define AM1805_REG_HOURS         0x03
#define AM1805_REG_DATE          0x04 // Число месяца
#define AM1805_REG_MONTH         0x05
#define AM1805_REG_YEAR          0x06
#define AM1805_REG_WEEKDAY       0x07 // День недели

#define AM1805_REG_STATUS        0x0F // Общий статус (прерывания)
#define AM1805_REG_CTRL1         0x10 // Управление 1 (Бит 0 = WRTC)
#define AM1805_REG_CAL_XT        0x14 // Калибровка кварца
#define AM1805_REG_OSC_CTRL      0x1C // Управление генератором (OSEL)
#define AM1805_REG_OSC_STAT      0x1D // Статус генератора (Бит 4 = OF)
#define AM1805_REG_KEY           0x1F // Регистр ключей доступа
#define AM1805_REG_BREF          0x21 // Порог переключения на батарею
#define AM1805_REG_BATMODE_IO    0x27 // Режим ввода-вывода при питании от батареи

/* --- Командные ключи и маски --- */
#define AM1805_KEY_OSC           0xA1 // Ключ для доступа к OSC_CTRL и OSC_STAT
#define AM1805_KEY_CONF          0x9D // Ключ для доступа к BREF и BATMODE_IO
#define AM1805_KEY_RESET         0x3C // Ключ для программного сброса
#define AM1805_CTRL1_24H         0x40 // Бит 6: 0 = 24-часовой формат

typedef enum {
    AM1805_OK = 0,
    AM1805_ERROR,
    AM1805_BUSY
} AM1805_Status;

typedef struct {
    uint8_t hours, minutes, seconds, hundredths;
    uint8_t day, month, weekday;
    uint16_t year;
    bool is_xt_active; // true, если работает внешний кварц (XT)
} AM1805_Time_t;

typedef struct {
    uint8_t cal_xt, osc_ctrl, bref, status, ctrl1, osc_stat;
} AM1805_Diag_t;

/* --- Основной интерфейс драйвера --- */
bool          AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id);
AM1805_Status AM1805_SoftwareReset(void);
AM1805_Status AM1805_GetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetTime(AM1805_Time_t *time);
AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour);
AM1805_Status AM1805_GetDiagnostics(AM1805_Diag_t *diag);
void          AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time);
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time);
/**
 * @brief Принудительная инициализация с проверкой пароля.
 * @param password Должен быть равен 0x55AA3C0F для выполнения.
 */
bool AM1805_ForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password);
bool AM1805_ResetAndForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password);

#endif
