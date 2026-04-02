/**
 * @file am1805.c
 * @brief Реализация функций драйвера AM1805AQ.
 * Кодировка: UTF-8.
 */

#include "am1805.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* --- Вспомогательные функции --- */
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/**
 * @brief Обертка над I2C с потокобезопасностью (FreeRTOS Mutex).
 */
static AM1805_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL) return AM1805_ERROR;
    
    if (i2c_mutex != NULL && osMutexAcquire(i2c_mutex, AM1805_MUTEX_TIMEOUT) != osOK) 
        return AM1805_BUSY;

    HAL_StatusTypeDef res;
    if (is_read)
        res = HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT);
    else
        res = HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT);

    if (i2c_mutex != NULL) osMutexRelease(i2c_mutex);
    
    return (res == HAL_OK) ? AM1805_OK : AM1805_ERROR;
}

/* ========================================================================== */
/* ИНИЦИАЛИЗАЦИЯ И СБРОС                                                      */
/* ========================================================================== */

/**
 * @brief Умная инициализация чипа. Проверяет регистры перед записью.
 * ВНИМАНИЕ: Не трогает регистр калибровки CAL_XT! Калибровка выполняется отдельно.
 */
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key;
    uint8_t current_val = 0;

    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, AM1805_I2C_TIMEOUT) != HAL_OK) 
        return false;

    // 1. УМНАЯ настройка осциллятора (запускаем генератор, если он остановлен)
    i2c_transfer(AM1805_REG_OSC_CTRL, &current_val, 1, true);
    if (current_val != 0x00) { 
        key = AM1805_KEY_OSC;
        i2c_transfer(AM1805_REG_KEY, &key, 1, false); 
        data = 0x00; 
        i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false);
    }

    // 2. УМНАЯ настройка порогов батареи (BREF = 2.1V для стабильной работы от 2.88V)
    i2c_transfer(AM1805_REG_BREF, &current_val, 1, true);
    if (current_val != 0xB0) { 
        key = AM1805_KEY_CONF;
        i2c_transfer(AM1805_REG_KEY, &key, 1, false); 
        data = 0xB0; 
        i2c_transfer(AM1805_REG_BREF, &data, 1, false);
    }

    // 3. Изоляция I/O шины (BATMODE_IO) - для стабильности I2C при работе от батареи
    key = AM1805_KEY_CONF;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false); 
    data = 0x00; 
    i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false);

    // 4. Разрешение записи времени (Включаем только WRTC, силовой ключ PWR2 выключен)
    data = 0x01; 
    i2c_transfer(AM1805_REG_CTRL1, &data, 1, false);

    return true;
}

/**
 * @brief Полный программный сброс чипа до заводских настроек.
 */
AM1805_Status AM1805_SoftwareReset(void) {
    uint8_t reset_cmd = 0x3C;
    AM1805_Status status = i2c_transfer(AM1805_REG_KEY, &reset_cmd, 1, false);
    
    // Отдаем процессорное время другим задачам RTOS, 
    // пока чип AM1805 перезагружает внутренние цепи (10 мс)
    osDelay(10); // Предполагается, что 1 тикет RTOS = 1 мс
    
    return status;
}

/* ========================================================================== */
/* РАБОТА СО ВРЕМЕНЕМ                                                         */
/* ========================================================================== */

AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t buffer[8];
    uint8_t status;
    
    if (i2c_transfer(AM1805_REG_HUNDREDTHS, buffer, 8, true) != AM1805_OK)
        return AM1805_ERROR;
        
    if (i2c_transfer(AM1805_REG_STATUS, &status, 1, true) != AM1805_OK)
        return AM1805_ERROR;

    time->hundredths = bcd2dec(buffer[0]);
    time->seconds    = bcd2dec(buffer[1] & 0x7F);
    time->minutes    = bcd2dec(buffer[2] & 0x7F);
    time->hours      = bcd2dec(buffer[3] & 0x3F);
    time->day        = bcd2dec(buffer[4] & 0x3F);
    time->month      = bcd2dec(buffer[5] & 0x1F);
    time->year       = 2000 + bcd2dec(buffer[6]);
    time->weekday    = buffer[7] & 0x07;
    
    time->is_xt_active = (status & 0x01) ? false : true; 

    return AM1805_OK;
}

AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buffer[8];
    uint8_t ctrl1 = 0x01;

    // Убеждаемся, что бит WRTC (разрешение записи) установлен
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false) != AM1805_OK) 
        return AM1805_ERROR;

    buffer[0] = dec2bcd(time->hundredths);
    buffer[1] = dec2bcd(time->seconds);
    buffer[2] = dec2bcd(time->minutes);
    buffer[3] = dec2bcd(time->hours);
    buffer[4] = dec2bcd(time->day);
    buffer[5] = dec2bcd(time->month);
    buffer[6] = dec2bcd(time->year % 100);
    buffer[7] = time->weekday;

    if (i2c_transfer(AM1805_REG_HUNDREDTHS, buffer, 8, false) != AM1805_OK)
        return AM1805_ERROR;

    return AM1805_OK;
}

/* ========================================================================== */
/* КАЛИБРОВКА И ДИАГНОСТИКА                                                   */
/* ========================================================================== */

AM1805_Status AM1805_SetCalibration(float adj_ppm) {
    int8_t cal_val = (int8_t)(adj_ppm / 1.907f);
    uint8_t data;
    uint8_t key;

    // Накладываем маску 0x7F (7 бит) для Two's complement
    data = (uint8_t)(cal_val & 0x7F); 

    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return AM1805_ERROR;
    
    return i2c_transfer(AM1805_REG_CAL_XT, &data, 1, false);
}

/**
 * @brief Тонкая настройка калибровки по уходу времени.
 * Вычисляется по формуле: 1 LSB регистра CAL_XT = 0.0068652 секунды в час.
 * Максимальный диапазон регистра: от -64 (0x40) до +63 (0x3F).
 */
AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour) {
    const float LSB_IN_SEC_PER_HOUR = 0.0068652f;
    
    // Вычисляем значение
    int16_t cal_val = (int16_t)(drift_sec_per_hour / LSB_IN_SEC_PER_HOUR);

    // Ограничиваем в допустимых пределах (-64 ... +63)
    if (cal_val > 63) {
        cal_val = 63;
    } else if (cal_val < -64) {
        cal_val = -64;
    }

    // Приводим к 7-битному формату Two's complement
    uint8_t data = (uint8_t)(cal_val & 0x7F); 

    uint8_t key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return AM1805_ERROR;
    
    return i2c_transfer(AM1805_REG_CAL_XT, &data, 1, false);
}

AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag) {
    i2c_transfer(AM1805_REG_CAL_XT,    &diag->cal_xt,   1, true);
    i2c_transfer(AM1805_REG_OSC_CTRL,  &diag->osc_ctrl, 1, true);
    i2c_transfer(AM1805_REG_BREF,      &diag->bref,     1, true);
    i2c_transfer(AM1805_REG_STATUS,    &diag->status,   1, true);
    i2c_transfer(AM1805_REG_CTRL1,     &diag->ctrl1,    1, true);
    i2c_transfer(AM1805_REG_OSC_STAT,  &diag->osc_stat, 1, true);
    return AM1805_OK;
}

void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time) return;
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u [%s]",
             time->year, time->month, time->day,
             time->hours, time->minutes, time->seconds,
             time->is_xt_active ? "XT" : "RC");
}

AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time) {
    AM1805_Time_t t = {0};
    char month_str[4] = {0};
    int day, year, hours, minutes, seconds;

    sscanf(time, "%d:%d:%d", &hours, &minutes, &seconds);
    t.hours = hours;
    t.minutes = minutes;
    t.seconds = seconds;

    sscanf(date, "%3s %d %d", month_str, &day, &year);
    t.day = day;
    t.year = year;

    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strncmp(month_str, months[i], 3) == 0) {
            t.month = i + 1;
            break;
        }
    }
    
    t.weekday = 1; 
    return t;
}