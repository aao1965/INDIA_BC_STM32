/**
 * @file am1805.c
 * @brief Реализация драйвера: принудительный XT, изоляция I/O, порог 2.1V/2.5V.
 * Кодировка: UTF-8.
 */

#include "am1805.h"

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* Вспомогательные функции преобразования BCD */
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/**
 * @brief Внутренняя функция передачи данных по I2C с использованием Мьютекса.
 */
static AM1805_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL) return AM1805_ERROR;
    if (i2c_mutex != NULL && osMutexAcquire(i2c_mutex, AM1805_MUTEX_TIMEOUT) != osOK) 
        return AM1805_BUSY;

    HAL_StatusTypeDef res = is_read ? 
        HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT) :
        HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT);

    if (i2c_mutex != NULL) osMutexRelease(i2c_mutex);
    return (res == HAL_OK) ? AM1805_OK : AM1805_ERROR;
}

/**
 * @brief Инициализация RTC с защитой от медленного падения напряжения (3 сек).
 */
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key;

    // Проверка наличия устройства на шине
    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, 100) != HAL_OK) return false;

    // 1. Настройка осциллятора + PWGT (Защита I/O при медленном падении питания)
    key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = 0x04; // OSEL=0 (XT), PWGT=1 (Аппаратная блокировка I/O шины)
    i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false);

    // 2. Настройка питания: порог BREF = 1011 (Fall 2.1V / Rise 2.5V)
    key = AM1805_KEY_CONF;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = (0x0B << 4); 
    i2c_transfer(AM1805_REG_BREF, &data, 1, false);

    // 3. Изоляция I2C шины при переходе на батарею (IOBM = 0)
    // Ключ пишем снова, т.к. он сбрасывается после каждой записи в защищенную область
    key = AM1805_KEY_CONF;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = 0x00; // IOBM = 0: полностью отключает I2C при питании от VBAT
    i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false);

    // 4. Включение Power Switch (PWR2) и разрешения записи (WRTC)
    data = 0x03; 
    i2c_transfer(AM1805_REG_CTRL1, &data, 1, false);

    // 5. Очистка флага Oscillator Failure (OF) после старта
    i2c_transfer(AM1805_REG_STATUS, &data, 1, true);
    data &= ~0x02; // Сброс бита OF
    i2c_transfer(AM1805_REG_STATUS, &data, 1, false);

    return true;
}

/**
 * @brief Получение текущего времени. Проверяет, активен ли кварц.
 */
AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t buf[7], osc_stat;
    if (i2c_transfer(AM1805_REG_SECONDS, buf, 7, true) != AM1805_OK) return AM1805_ERROR;

    time->seconds    = bcd2dec(buf[0] & 0x7F);
    time->minutes    = bcd2dec(buf[1] & 0x7F);
    time->hours      = bcd2dec(buf[2] & 0x3F);
    time->day        = bcd2dec(buf[3] & 0x3F);
    time->month      = bcd2dec(buf[4] & 0x1F);
    time->year       = 2000 + bcd2dec(buf[5]);
    time->weekday    = bcd2dec(buf[6] & 0x07);

    // Проверка бита OMODE (0 = XT активен, 1 = RC активен)
    if (HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_OSC_STAT, 1, &osc_stat, 1, 100) == HAL_OK) {
        time->is_xt_active = !(osc_stat & (1 << 4)); 
    }
    return AM1805_OK;
}

/**
 * @brief Установка времени. Запись с 0-го регистра сбрасывает "сотые", синхронизируя старт.
 */
AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buf[8];
    buf[0] = 0; //Hundredths = 0
    buf[1] = dec2bcd(time->seconds);
    buf[2] = dec2bcd(time->minutes);
    buf[3] = dec2bcd(time->hours);
    buf[4] = dec2bcd(time->day);
    buf[5] = dec2bcd(time->month);
    buf[6] = dec2bcd(time->year % 100);
    buf[7] = dec2bcd(time->weekday);
    return i2c_transfer(AM1805_REG_HUNDREDTHS, buf, 8, false);
}

/**
 * @brief Точная цифровая калибровка. 1 шаг = 1.907 ppm.
 * @param adj_ppm Положительное значение ускоряет, отрицательное замедляет.
 */
AM1805_Status AM1805_SetCalibration(float adj_ppm) {
    int8_t steps = (int8_t)(adj_ppm / 1.907f);
    if (steps > 63)  steps = 63;
    if (steps < -64) steps = -64;
    
    uint8_t reg_val = (uint8_t)(steps & 0x7F); 
    uint8_t key = AM1805_KEY_CONF;
    
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    return i2c_transfer(AM1805_REG_CAL_XT, &reg_val, 1, false);
}

/**
 * @brief Форматирование даты и времени в строку.
 */
void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time) return;
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u [%s]",
             time->year, time->month, time->day,
             time->hours, time->minutes, time->seconds,
             time->is_xt_active ? "XT" : "RC");
}

/**
 * @brief Чтение диагностических регистров.
 */
AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag) {
    i2c_transfer(AM1805_REG_CAL_XT,    &diag->cal_xt,   1, true);
    i2c_transfer(AM1805_REG_OSC_CTRL,  &diag->osc_ctrl, 1, true);
    i2c_transfer(AM1805_REG_BREF,      &diag->bref,     1, true);
    i2c_transfer(AM1805_REG_STATUS,    &diag->status,   1, true);
    i2c_transfer(AM1805_REG_CTRL1,     &diag->ctrl1,    1, true);
    return AM1805_OK;
}

/**
 * @brief Парсинг времени макросов компиляции __DATE__ и __TIME__.
 */
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time) {
    AM1805_Time_t t = {0};
    char m_str[4]; int d, y, hh, mm, ss;
    const char *m_list[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    if (sscanf(date, "%s %d %d", m_str, &d, &y) == 3 && sscanf(time, "%d:%d:%d", &hh, &mm, &ss) == 3) {
        for (int i=0; i<12; i++) {
            if (!strcmp(m_str, m_list[i])) { t.month = i + 1; break; }
        }
        t.day = d; t.year = y; t.hours = hh; t.minutes = mm; t.seconds = ss;
    }
    return t;
}
