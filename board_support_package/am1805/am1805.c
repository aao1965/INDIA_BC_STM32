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
 * @brief Инициализация RTC AM1805AQ (Smart версия 1.2)
 * Настроено под: Кварц 7pF, Батарея 2.89V, медленное падение VCC (3 сек).
 * Кодировка: UTF-8.
 */
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key;

    // 0. Проверка физического отклика чипа по адресу 0x69
    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, 100) != HAL_OK) {
        return false;
    }

    // 1. Настройка осциллятора (XT) и защиты ввода-вывода (PWGT)
    // ВАЖНО: Устанавливаем OUT = 01 (6pF) для кварца 7pF (ABS06)
    key = AM1805_KEY_OSC; // 0xA1
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
	/*
	 Биты регистра 0x1C:
	 OSEL (7) = 0 (Внешний кварц XT)
	 PWGT (2) = 1 (Аппаратная защита I/O)
	 OUT (1:0) = 00 (Нагрузочная емкость 12.5 pF)
	 Дает стабильный ход, но замедляет на -222 ppm. Исправим это калибровкой.
	 */
	data = 0x04; // Возвращаем 12.5 pF
	i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false);

    // 2. Настройка порога переключения на батарею (BREF)
    key = AM1805_KEY_CONF; // 0x9D
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    /* 1011 (0x0B) -> Fall 2.1V / Rise 2.5V (Идеально для батарейки 2.89V) */
    data = (0x0B << 4);
    i2c_transfer(AM1805_REG_BREF, &data, 1, false);

    // 3. Изоляция I2C шины при работе от батареи (IOBM)
    // Ключ пишем снова, так как он сбрасывается после каждой записи!
    key = AM1805_KEY_CONF; // 0x9D
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    /* IOBM = 0: полностью отключает I2C на VBAT (защита от мусора при падении VCC 3 сек) */
    data = 0x00;
    i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false);

    // 4. Включение Battery Switch (PWR2) и разрешения записи времени (WRTC)
    /* 0x03 -> PWR2=1, WRTC=1 */
    data = 0x03;
    i2c_transfer(AM1805_REG_CTRL1, &data, 1, false);

    // 5. Очистка флага Oscillator Failure (OF) в регистре 0x1D
    // Это сбросит вашу прошлую ошибку "osc_status = 34"
    key = AM1805_KEY_OSC; // 0xA1
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    /* Сбрасываем биты OF (1) и ACF (0). OMODE(4) и LKO2(5) не изменятся. */
    data = 0x00;
    i2c_transfer(AM1805_REG_OSC_STAT, &data, 1, false);

    // Финальная проверка: не вернулся ли флаг ошибки OF мгновенно?
    uint8_t check_stat = 0;
    i2c_transfer(AM1805_REG_OSC_STAT, &check_stat, 1, true);
    if (check_stat & 0x02) {
        // Если OF опять = 1, значит кварц всё еще не заводится стабильно
        return false;
    }

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
/*AM1805_Status AM1805_SetCalibration(float adj_ppm) {
    int8_t steps = (int8_t)(adj_ppm / 1.907f);
    if (steps > 63)  steps = 63;
    if (steps < -64) steps = -64;
    
    uint8_t reg_val = (uint8_t)(steps & 0x7F); 
    uint8_t key = AM1805_KEY_CONF;
    
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    return i2c_transfer(AM1805_REG_CAL_XT, &reg_val, 1, false);
}*/
AM1805_Status AM1805_SetCalibration(float adj_ppm) {
    uint8_t reg_val;
    // Режим Coarse (грубый) для коррекции > 120 ppm
    if (adj_ppm > 120.0f || adj_ppm < -120.0f) {
        int8_t steps = (int8_t)(adj_ppm / 3.814f);
        if (steps > 63) steps = 63;
        if (steps < -64) steps = -64;
        reg_val = (uint8_t)(steps & 0x7F) | 0x80; // Включаем Bit 7 (CMDX)
    } else {
        int8_t steps = (int8_t)(adj_ppm / 1.907f);
        if (steps > 63) steps = 63;
        if (steps < -64) steps = -64;
        reg_val = (uint8_t)(steps & 0x7F); // Обычный режим
    }
    uint8_t key = AM1805_KEY_CONF; // 0x9D
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
    i2c_transfer(AM1805_REG_OSC_STAT,  &diag->osc_stat, 1, true); // Добавили чтение 0x1D
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
