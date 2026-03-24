/**
 * @file am1805.c
 * @brief Smart Implementation: Forced XT, 20pF Load, No RC Fallback.
 */

#include "am1805.h"

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* --- Private Helpers --- */
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

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
 * @brief Smart Initialization: Forced XT, 20pF, No RC Fallback.
 */
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key, ctrl1;

    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, 100) != HAL_OK) return false;

    // 1. Принудительный выбор кварца (XT), 20pF нагрузка, запрет RC-fallback
    key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = 0x10; // OSEL=0 (XT), OUT=01 (20pF), FOS=0 (No Fallback)
    i2c_transfer(AM1805_REG_OCTRL, &data, 1, false);

    // 2. Сброс калибровки и очистка флага Oscillator Failure
    key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = 0x00;
    i2c_transfer(AM1805_REG_CAL_XT, &data, 1, false);

    i2c_transfer(AM1805_REG_STATUS, &data, 1, true);
    data &= ~(1 << 2); // Сброс бита OF
    i2c_transfer(AM1805_REG_STATUS, &data, 1, false);

    // 3. Активация бита WRTC (разрешение записи)
    i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true);
    if (!(ctrl1 & 0x01)) {
        key = AM1805_KEY_OSC;
        i2c_transfer(AM1805_REG_KEY, &key, 1, false);
        ctrl1 |= 0x01;
        i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false);
    }

#ifdef AM1805_ENABLE_CLKOUT
    key = AM1805_KEY_EXT;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    data = 0x04; // Push-pull вывод
    i2c_transfer(AM1805_REG_CTRL2, &data, 1, false);
    data = 0x80 | AM1805_CLKOUT_FREQ;
    i2c_transfer(AM1805_REG_SQW, &data, 1, false);
#endif
    return true;
}

AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t buf[7], ostat;
    if (i2c_transfer(AM1805_REG_HUNDREDTHS, buf, 7, true) != AM1805_OK) return AM1805_ERROR;

    time->hundredths = bcd2dec(buf[0]);
    time->seconds    = bcd2dec(buf[1] & 0x7F);
    time->minutes    = bcd2dec(buf[2] & 0x7F);
    time->hours      = bcd2dec(buf[3] & 0x3F);
    time->day        = bcd2dec(buf[4] & 0x3F);
    time->month      = bcd2dec(buf[5] & 0x1F);
    time->year       = 2000 + bcd2dec(buf[6]);

    if (i2c_transfer(AM1805_REG_OSTAT, &ostat, 1, true) == AM1805_OK) {
        time->is_xt_active = !(ostat & (1 << 4)); // OMODE bit: 0 = XT, 1 = RC
    }
    return AM1805_OK;
}

AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buf[8], key = AM1805_KEY_EXT;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);

    buf[0] = dec2bcd(time->hundredths);
    buf[1] = dec2bcd(time->seconds);
    buf[2] = dec2bcd(time->minutes);
    buf[3] = dec2bcd(time->hours);
    buf[4] = dec2bcd(time->day);
    buf[5] = dec2bcd(time->month);
    buf[6] = dec2bcd(time->year % 100);
    buf[7] = 0;

    return i2c_transfer(AM1805_REG_HUNDREDTHS, buf, 8, false);
}

AM1805_Status AM1805_AutoCalibrate(float temp) {
#if (AM1805_USE_AUTOCALIB == 0)
    return AM1805_OK;
#else
    if (temp < RTC_MIN_VALID_TEMP) return AM1805_ERROR;
    float dt = temp - 25.0f;
    int8_t adj = (int8_t)((0.034f * dt * dt) / 2.03f);
    uint8_t key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    return i2c_transfer(AM1805_REG_CAL_XT, (uint8_t*)&adj, 1, false);
#endif
}

AM1805_Status AM1805_ReadDiagnostic(AM1805_Diag_t *diag) {
    i2c_transfer(AM1805_REG_CAL_XT, &diag->cal_xt, 1, true);
    i2c_transfer(AM1805_REG_OCTRL,  &diag->octrl,  1, true);
    i2c_transfer(AM1805_REG_OSTAT,  &diag->ostat,  1, true);
    i2c_transfer(AM1805_REG_STATUS, &diag->status, 1, true);
    return i2c_transfer(AM1805_REG_CTRL1, &diag->ctrl1, 1, true);
}

void AM1805_FormatTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time || len < 15) return;
    snprintf(buf, len, "%02u:%02u:%02u [%s]", 
             time->hours, time->minutes, time->seconds, 
             time->is_xt_active ? "XT" : "RC");
}

void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time || len < 30) return;
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u [%s]",
             time->year, time->month, time->day,
             time->hours, time->minutes, time->seconds,
             time->is_xt_active ? "XT" : "RC");
}

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
