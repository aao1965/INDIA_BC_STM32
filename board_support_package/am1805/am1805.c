/**
 * @file am1805.c
 * @brief Реализация функций драйвера AM1805AQ.
 * Кодировка: UTF-8.
 */

#include "am1805.h"

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* --- Вспомогательные функции преобразования BCD --- */
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/**
 * @brief Обертка над I2C с использованием мьютекса для потокобезопасности.
 */
static AM1805_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL) return AM1805_ERROR;
    
    // Захват мьютекса (ожидание освобождения шины другими задачами)
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

/**
 * @brief Умная инициализация RTC. 
 * Решает проблему медленного падения напряжения и калибровки кварца 12.5pF.
 */
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key;

    // Проверка физического присутствия чипа на шине I2C
    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, 100) != HAL_OK) return false;

    /* 1. Настройка осциллятора (XT) и защиты ввода-вывода (PWGT) */
    key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    /* 
     * data = 0x04:
     * OSEL (7) = 0 (Выбор внешнего кварца XT)
     * PWGT (2) = 1 (Аппаратная блокировка I/O шины при нестабильном питании)
     * OUT (1:0) = 00 (Емкость 12.5 pF для кварца FC-12M)
     */
    data = 0x04; 
    i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false);

    /* 2. Настройка порога переключения на батарею (BREF) */
    key = AM1805_KEY_CONF;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    /* 
     * data = 0xB0 (1011 в старшем ниббле):
     * Порог Fall (падение): 2.1V
     * Порог Rise (восстановление): 2.5V 
     * Оптимально для батарейки 2.89V и основной линии 3.3V
     */
    data = (0x0B << 4); 
    i2c_transfer(AM1805_REG_BREF, &data, 1, false);

    /* 3. Изоляция I2C шины при работе от батареи (IOBM) */
    key = AM1805_KEY_CONF; // Повторный ввод ключа необходим!
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    /* 
     * data = 0x00 (IOBM = 0):
     * Полное отключение интерфейса I2C при питании от батареи.
     * Защищает от мусора на шине, когда STM32 "умирает" при падении VCC (3 сек).
     */
    data = 0x00; 
    i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false);

    /* 4. Включение переключателя питания и доступа к записи (WRTC) */
    /* data = 0x03: PWR2 = 1 (включить автопереход), WRTC = 1 (разрешить запись времени) */
    data = 0x03; 
    i2c_transfer(AM1805_REG_CTRL1, &data, 1, false);

    /* 5. Очистка истории ошибок осциллятора (Сброс флага OF) */
    key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = 0x00; // Сброс битов OF (сбой кварца) и ACF (сбой автокалибровки)
    i2c_transfer(AM1805_REG_OSC_STAT, &data, 1, false);

    return true;
}


/**
 * @brief Чтение времени. Также обновляет флаг XT/RC.
 */
AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t buf[7], osc_stat;
    // Читаем со второго регистра (Seconds), игнорируя сотые
    if (i2c_transfer(AM1805_REG_SECONDS, buf, 7, true) != AM1805_OK) return AM1805_ERROR;

    time->seconds    = bcd2dec(buf[0] & 0x7F);
    time->minutes    = bcd2dec(buf[1] & 0x7F);
    time->hours      = bcd2dec(buf[2] & 0x3F);
    time->day        = bcd2dec(buf[3] & 0x3F);
    time->month      = bcd2dec(buf[4] & 0x1F);
    time->year       = 2000 + bcd2dec(buf[5]);
    time->weekday    = bcd2dec(buf[6] & 0x07);

    // Проверяем бит OMODE (0 = кварц активен, 1 = чип ушел на RC из-за сбоя)
    if (HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_OSC_STAT, 1, &osc_stat, 1, 100) == HAL_OK) {
        time->is_xt_active = !(osc_stat & (1 << 4)); 
    }
    return AM1805_OK;
}

/**
 * @brief Установка времени. 
 * ВАЖНО: Запись начинается с регистра 0x00 (hundredths), что сбрасывает 
 * внутренние делители чипа для точной синхронизации старта секунды.
 */
AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buf[8];
    buf[0] = 0; //Hundredths = 0 (сброс фазы секунды)
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
 * @brief Цифровая калибровка хода часов.
 * Поддерживает два режима: Normal (до 120ppm) и Coarse (до 240ppm).
 * @param adj_ppm: положительное ускоряет, отрицательное замедляет.
 */
AM1805_Status AM1805_SetCalibration(float adj_ppm) {
    uint8_t reg_val;
    
    // Если требуется большая коррекция (> 120 ppm), включаем грубый режим (бит CMDX)
    if (adj_ppm > 120.0f || adj_ppm < -120.0f) {
        int8_t steps = (int8_t)(adj_ppm / 3.814f); // 1 шаг грубого режима = 3.814 ppm
        if (steps > 63)  steps = 63;
        if (steps < -64) steps = -64;
        reg_val = (uint8_t)(steps & 0x7F) | 0x80; // Установка бита 7 (CMDX)
    } else {
        int8_t steps = (int8_t)(adj_ppm / 1.907f); // 1 шаг нормального режима = 1.907 ppm
        if (steps > 63)  steps = 63;
        if (steps < -64) steps = -64;
        reg_val = (uint8_t)(steps & 0x7F);        // CMDX = 0
    }

    uint8_t key = AM1805_KEY_CONF; 
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    return i2c_transfer(AM1805_REG_CAL_XT, &reg_val, 1, false);
}

/**
 * @brief Чтение диагностических регистров для проверки состояния железа.
 */
AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag) {
    i2c_transfer(AM1805_REG_CAL_XT,    &diag->cal_xt,   1, true);
    i2c_transfer(AM1805_REG_OSC_CTRL,  &diag->osc_ctrl, 1, true);
    i2c_transfer(AM1805_REG_BREF,      &diag->bref,     1, true);
    i2c_transfer(AM1805_REG_STATUS,    &diag->status,   1, true);
    i2c_transfer(AM1805_REG_CTRL1,     &diag->ctrl1,    1, true);
    i2c_transfer(AM1805_REG_OSC_STAT,  &diag->osc_stat, 1, true);
    return AM1805_OK;
}

/**
 * @brief Утилита для формирования красивой строки даты/времени.
 */
void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time) return;
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u [%s]",
             time->year, time->month, time->day,
             time->hours, time->minutes, time->seconds,
             time->is_xt_active ? "XT" : "RC");
}

/**
 * @brief Парсинг времени из макросов компилятора.
 * Полезно для установки начального времени при первом запуске устройства.
 */
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time_str) {
    AM1805_Time_t t = {0};
    char m_str[4]; int d, y, hh, mm, ss;
    const char *m_list[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    
    if (sscanf(date, "%s %d %d", m_str, &d, &y) == 3 && sscanf(time_str, "%d:%d:%d", &hh, &mm, &ss) == 3) {
        for (int i=0; i<12; i++) {
            if (!strcmp(m_str, m_list[i])) { t.month = i + 1; break; }
        }
        t.day = d; t.year = y; t.hours = hh; t.minutes = mm; t.seconds = ss;
    }
    return t;
}
