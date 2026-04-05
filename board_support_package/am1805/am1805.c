#include "am1805.h"
#include <stdio.h>
#include <string.h>

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* Вспомогательные функции конвертации BCD <-> DEC */
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/**
 * @brief Низкоуровневая передача данных по I2C с блокировкой мьютекса.
 */
static AM1805_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL) return AM1805_ERROR;
    
    // Пытаемся захватить мьютекс шины, если он передан
    if (i2c_mutex != NULL && osMutexAcquire(i2c_mutex, AM1805_MUTEX_TIMEOUT) != osOK) return AM1805_BUSY;

    HAL_StatusTypeDef res = is_read ? 
        HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT) :
        HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT);

    if (i2c_mutex != NULL) osMutexRelease(i2c_mutex);
    return (res == HAL_OK) ? AM1805_OK : AM1805_ERROR;
}

/**
 * @brief Умная инициализация. Проверяет, идут ли часы и исправен ли кварц.
 */
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key, ctrl1, osc_stat;

    // Проверяем физическое наличие чипа на шине
    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, AM1805_I2C_TIMEOUT) != HAL_OK) return false;

    // ШАГ 1: Проверяем программный флаг WRTC (0x10) и железный статус генератора OF (0x1D)
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true) != AM1805_OK) return false;
    if (i2c_transfer(AM1805_REG_OSC_STAT, &osc_stat, 1, true) != AM1805_OK) return false;

    /**
     * ЛОГИКА ПРОПУСКА ИНИЦИАЛИЗАЦИИ:
     * 1. (ctrl1 & 0x01) == 0  => Бит WRTC сброшен. Значит, мы уже настраивали чип ранее.
     * 2. (osc_stat & 0x10) == 0 => Бит OF (Oscillator Failure) чист. Генератор не останавливался.
     * Если ты перепаивал кварц, бит OF будет равен 1. Мы НЕ выйдем здесь и пойдем на полную настройку.
     */
    if (((ctrl1 & 0x01) == 0) && ((osc_stat & 0x10) == 0)) {
        return true; 
    }

    // --- ХОЛОДНЫЙ СТАРТ / СБОЙ КВАРЦА ---

    // ШАГ 2: Сброс флага ошибки генератора (OF)
    key = AM1805_KEY_OSC; // Разблокируем доступ к регистрам генератора
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x00; // Очищаем статус (сбрасываем OF), чтобы начать мониторинг заново
    if (i2c_transfer(AM1805_REG_OSC_STAT, &data, 1, false) != AM1805_OK) return false;

    // ШАГ 3: Принудительный выбор XT (кварцевого) генератора (0x1C)
    key = AM1805_KEY_OSC;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x00; // OSEL = 0 включает 32.768 kHz XT кварц
    if (i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false) != AM1805_OK) return false;

    // ШАГ 4: Настройка порога переключения на батарею (BREF)
    key = AM1805_KEY_CONF; 
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0xB0; // Установка порога 2.1V для переключения питания
    if (i2c_transfer(AM1805_REG_BREF, &data, 1, false) != AM1805_OK) return false;

    // ШАГ 5: Разрешение работы интерфейса I2C при питании от батареи (IOBM)
    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x80; // IOBM = 1 позволяет "будить" чип по шине, когда основное питание пропало
    if (i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false) != AM1805_OK) return false;

    // ШАГ 6: Финальный запуск: сброс WRTC и установка 24-часового режима (0x10)
    ctrl1 &= ~(0x01 | AM1805_CTRL1_24H); 
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false) != AM1805_OK) return false;

    return true;
}

/**
 * @brief Чтение текущего времени и статуса генератора.
 */
AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t buffer[8], osc_stat;
    // Читаем пачку регистров времени (0x00 - 0x07) за один раз
    if (i2c_transfer(AM1805_REG_HUNDREDTHS, buffer, 8, true) != AM1805_OK) return AM1805_ERROR;
    if (i2c_transfer(AM1805_REG_OSC_STAT, &osc_stat, 1, true) != AM1805_OK) return AM1805_ERROR;

    time->hundredths = bcd2dec(buffer[0]);
    time->seconds    = bcd2dec(buffer[1] & 0x7F);
    time->minutes    = bcd2dec(buffer[2] & 0x7F);
    time->hours      = bcd2dec(buffer[3] & 0x3F);
    time->day        = bcd2dec(buffer[4] & 0x3F);
    time->month      = bcd2dec(buffer[5] & 0x1F);
    time->year       = 2000 + bcd2dec(buffer[6]);
    time->weekday    = buffer[7] & 0x07;
    
    // Проверка бита XPF (бит 5): 1 = работает именно кварц (XT)
    time->is_xt_active = (osc_stat & 0x20) ? true : false; 
    return AM1805_OK;
}

/**
 * @brief Установка времени с использованием паттерна Read-Modify-Write.
 */
AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buffer[8], ctrl1;
    AM1805_Status status;
    
    // Сначала читаем текущий CTRL1, чтобы не затереть другие настройки (например, прерывания)
    if ((status = i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true)) != AM1805_OK) return status;

    ctrl1 |= 0x01; // Устанавливаем WRTC=1 (разрешаем запись в регистры времени)
    if ((status = i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false)) != AM1805_OK) return status;

    buffer[0] = dec2bcd(time->hundredths);
    buffer[1] = dec2bcd(time->seconds);
    buffer[2] = dec2bcd(time->minutes);
    buffer[3] = dec2bcd(time->hours);
    buffer[4] = dec2bcd(time->day);
    buffer[5] = dec2bcd(time->month);
    buffer[6] = dec2bcd(time->year % 100);
    buffer[7] = time->weekday & 0x07;

    // Пишем всё время пачкой
    if ((status = i2c_transfer(AM1805_REG_HUNDREDTHS, buffer, 8, false)) != AM1805_OK) return status;

    ctrl1 &= ~0x01; // Сбрасываем WRTC=0 (запускаем счет и блокируем запись)
    return i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false);
}

/**
 * @brief Чтение диагностических регистров в структуру.
 */
AM1805_Status AM1805_GetDiagnostics(AM1805_Diag_t *diag) {
    AM1805_Status status;
    if ((status = i2c_transfer(AM1805_REG_CAL_XT, &diag->cal_xt, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_OSC_CTRL, &diag->osc_ctrl, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_BREF, &diag->bref, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_STATUS, &diag->status, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_CTRL1, &diag->ctrl1, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_OSC_STAT, &diag->osc_stat, 1, true)) != AM1805_OK) return status;
    return AM1805_OK;
}

/**
 * @brief Разбор макросов времени компиляции __DATE__ и __TIME__.
 */
AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time) {
    AM1805_Time_t t = {0};
    char m_str[4]; 
    int d, y, h, mn, s;

    // Парсим "HH:MM:SS"
    sscanf(time, "%d:%d:%d", &h, &mn, &s);
    t.hours = h; t.minutes = mn; t.seconds = s;

    // Парсим "Mmm DD YYYY"
    sscanf(date, "%3s %d %d", m_str, &d, &y);
    t.day = d; t.year = y;

    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    t.month = 1; // Защита от некорректного парсинга
    for (int i = 0; i < 12; i++) {
        if (strncmp(m_str, months[i], 3) == 0) { 
            t.month = i + 1; 
            break; 
        }
    }
    t.weekday = 1; 
    return t;
}

/**
 * @brief Принудительная настройка всех критических регистров с защитой.
 */
bool AM1805_ForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password) {
    // Проверка "пароля" перед выполнением деструктивных или важных действий
    if (password != 0x55AA3C0F) {
        return false; 
    }

    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key, ctrl1;

    // 1. Проверка физического присутствия чипа на шине
    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, AM1805_I2C_TIMEOUT) != HAL_OK) return false;

    // 2. Сброс флага ошибки генератора (OF) в регистре 0x1D
    key = AM1805_KEY_OSC; 
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x00; 
    if (i2c_transfer(AM1805_REG_OSC_STAT, &data, 1, false) != AM1805_OK) return false;

    // 3. Выбор внешнего кварца XT (32.768 кГц) в регистре 0x1C
    key = AM1805_KEY_OSC;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x00; // OSEL = 0
    if (i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false) != AM1805_OK) return false;

    // 4. Установка порога переключения на батарею (BREF = 2.1V)
    key = AM1805_KEY_CONF; 
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0xB0; 
    if (i2c_transfer(AM1805_REG_BREF, &data, 1, false) != AM1805_OK) return false;

    // 5. Разрешение доступа по I2C при работе от батареи (IOBM)
    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x80; 
    if (i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false) != AM1805_OK) return false;

    // 6. Установка режима 24h и запуск счета (сброс WRTC)
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true) != AM1805_OK) return false;
    ctrl1 &= ~(0x01 | AM1805_CTRL1_24H); 
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false) != AM1805_OK) return false;

    // 6. Жесткая очистка Control1 (сброс PWR2, OUT, WRTC и установка режима 24h)
	/*ctrl1 = 0x00;
	if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false) != AM1805_OK)	return false;*/

    return true;
}

/* ========================================================================== */
/* ВОССТАНОВЛЕННЫЕ ФУНКЦИИ                                                    */
/* ========================================================================== */

/**
 * @brief Программный сброс чипа.
 */
AM1805_Status AM1805_SoftwareReset(void) {
    uint8_t reset_cmd = AM1805_KEY_RESET;
    AM1805_Status status = i2c_transfer(AM1805_REG_KEY, &reset_cmd, 1, false);
    osDelay(10);
    uint8_t clear_status = 0x00;
    i2c_transfer(AM1805_REG_STATUS, &clear_status, 1, false);
    return status;
}

/**
 * @brief Полный цикл восстановления: программный сброс + принудительная настройка.
 */
bool AM1805_ResetAndForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password) {
    // 1. Инициализируем указатели ПЕРЕД сбросом, иначе i2c_transfer не сработает
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;

    // 2. Делаем программный сброс (чистим Control1 и Status)
    if (AM1805_SoftwareReset() != AM1805_OK) {
        return false;
    }

    // 3. Запускаем жесткую инициализацию с проверкой пароля
    return AM1805_ForceInit(hi2c_ptr, mutex_id, 0x55AA3C0F);
}

/**
 * @brief Настройка калибровки по реальному уходу времени с автовыбором режима.
 * * @param drift_sec_per_hour Уход часов в секундах за 1 час.
 * (+) часы спешат (нужно замедлять).
 * (-) часы отстают (нужно ускорять).
 * * @return AM1805_Status AM1805_OK при успешной записи по I2C.
 */
AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour) {
    /*
     * КАК РАССЧИТЫВАЕТСЯ КОНСТАНТА 0.0073242:
     * 1. Кварц работает на частоте 32 768 Гц.
     * 2. В режиме Normal (CMDX = 0) 1 шаг калибровки (1 LSB) означает
     * добавление или пропуск ровно 1 импульса кварца каждые 15 секунд.
     * 3. За 1 час (3600 секунд) происходит 3600 / 15 = 240 таких корректировок.
     * 4. Итого за час мы корректируем время на 240 импульсов.
     * 5. В секундах это: 240 импульсов / 32768 Гц = 0.00732421875 сек/час.
     * * В режиме Coarse (CMDX = 1) шаг удваивается: 1 импульс каждые 7.5 секунд.
     */
    const float LSB_NORMAL_SEC_PER_HOUR = 0.0073242f;

    bool coarse_mode = false;
    float current_lsb = LSB_NORMAL_SEC_PER_HOUR;

    // Предварительный расчет шагов для точного режима
    // Формула с минусом: если часы спешат (drift > 0), нам нужно ОТРИЦАТЕЛЬНОЕ
    // значение шагов, чтобы чип "проглатывал" импульсы и замедлял счет.
    float steps = -drift_sec_per_hour / current_lsb;

    // Проверяем, помещается ли значение в 7-битный регистр (от -64 до +63)
    if (steps > 63.0f || steps < -64.0f) {
        // Уход слишком большой. Включаем грубый режим (Coarse Mode)
        coarse_mode = true;
        current_lsb *= 2.0f;                       // Шаг удваивается
        steps = -drift_sec_per_hour / current_lsb; // Пересчитываем шаги
    }

    int16_t cal_val = (int16_t)steps;

    // Защита от переполнения (ограничиваем экстремальные значения)
    if (cal_val > 63) cal_val = 63;
    else if (cal_val < -64) cal_val = -64;

    // Формируем байт данных. Приведение отрицательного числа к int8_t
    // и наложение маски 0x7F автоматически дает правильный формат Two's complement
    // для битов 0-6 (OFFSETX).
    uint8_t data = (uint8_t)((int8_t)cal_val & 0x7F);

    // Если включен грубый режим, устанавливаем старший бит CMDX (Бит 7 = 1)
    if (coarse_mode) {
        data |= 0x80;
    }

    uint8_t key = AM1805_KEY_CONF;

    // Открываем доступ к регистру CAL_XT и пишем значение
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return AM1805_ERROR;
    return i2c_transfer(AM1805_REG_CAL_XT, &data, 1, false);
}

/**
 * @brief Форматирование времени в строку.
 */
void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time) return;
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u [%s]",
             time->year, time->month, time->day,
             time->hours, time->minutes, time->seconds,
             time->is_xt_active ? "XT" : "RC/ERR");
}
