/**
 * @file am1805.c
 * @brief Implementation with Mutex protection, 32kHz output, and BCD decoding.
 * @encoding UTF-8
 */

#include "am1805.h"

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* --- Private Helpers --- */
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/**
 * @brief Internal thread-safe I2C transfer helper.
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
 * @brief Initialize RTC with XT priority, Auto RC fallback and Debug CLKOUT.
 */
bool AM1805_Init(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key;

    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, 100) != HAL_OK) return false;

    /* 1. Oscillator Setup (Key 0xA1 for OCTRL 0x1C) */
    key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    i2c_transfer(AM1805_REG_OCTRL, &data, 1, true);
    data &= ~(1 << 7); /* OSEL = 0 (XT Crystal) */
    data |= (1 << 3);  /* FOS = 1 (Automatic RC Fallback) */
    i2c_transfer(AM1805_REG_OCTRL, &data, 1, false);

    /* 2. Reset Oscillator Failure (OF) flag and Enable RTC Writing (WRTC) */
    i2c_transfer(AM1805_REG_STATUS, &data, 1, true);
    data &= ~(1 << 2); /* Clear OF bit */
    i2c_transfer(AM1805_REG_STATUS, &data, 1, false);

    i2c_transfer(AM1805_REG_CTRL1, &data, 1, true);
    data |= (1 << 0);  /* WRTC = 1 */
    i2c_transfer(AM1805_REG_CTRL1, &data, 1, false);

    /* 3. Debug CLKOUT Configuration (Pin 8) */
#ifdef AM1805_ENABLE_CLKOUT
    key = AM1805_KEY_EXT;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);

    /* Set Pin 8 to Push-Pull mode (Control2) */
    i2c_transfer(AM1805_REG_CTRL2, &data, 1, true);
    data |= (1 << 2);  /* OUT2P = 1 */
    i2c_transfer(AM1805_REG_CTRL2, &data, 1, false);

    /* Enable SQW at defined frequency */
    data = 0x80 | AM1805_CLKOUT_FREQ; 
    i2c_transfer(AM1805_REG_SQW, &data, 1, false);
#endif

    return true;
}

/**
 * @brief Smart Initialize for AM1805AQ.
 * Checks current oscillator status and only reconfigures if RTC is on RC fallback
 * or if write access (WRTC) is disabled.
 * * @param hi2c_ptr Pointer to HAL I2C handle
 * @param mutex_id Handle to the shared I2C mutex (from DS1621)
 * @return true if successful, false on I2C failure
 */
 
bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key, ostat, ctrl1;

    // Physical check
    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, 100) != HAL_OK) return false;

    /* 1. Read Oscillator Status (0x1D). Bit 4 (OMODE): 0=XT, 1=RC */
    if (i2c_transfer(AM1805_REG_OSTAT, &ostat, 1, true) != AM1805_OK) return false;

    if (ostat & (1 << 4)) {
        /* SITUATION: Running on RC fallback - We MUST force XT */
        
        // Unlock Oscillator registers (Key 0xA1)
        key = AM1805_KEY_OSC;
        i2c_transfer(AM1805_REG_KEY, &key, 1, false);
        
        // Set OSEL=0 (XT) and FOS=1 (Auto RC fallback) in OCTRL (0x1C)
        i2c_transfer(AM1805_REG_OCTRL, &data, 1, true);
        data &= ~(1 << 7); // OSEL = 0
        data |= (1 << 3);  // FOS = 1
        i2c_transfer(AM1805_REG_OCTRL, &data, 1, false);
        
        // Clear OF (Oscillator Failure) bit in Status (0x0F)
        i2c_transfer(AM1805_REG_STATUS, &data, 1, true);
        data &= ~(1 << 2); 
        i2c_transfer(AM1805_REG_STATUS, &data, 1, false);
    }

    /* 2. Check Write RTC bit (WRTC). Must be 1 to allow SetTime/Calibration */
    i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true);
    if (!(ctrl1 & 0x01)) {
        key = AM1805_KEY_OSC; // Key 0xA1 is also used for CTRL1
        i2c_transfer(AM1805_REG_KEY, &key, 1, false);
        ctrl1 |= 0x01; // Set WRTC
        i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false);
    }

    /* 3. Debug CLKOUT (Pin 8) - Only if enabled in header */
#ifdef AM1805_ENABLE_CLKOUT
    // Hardware verification: Enable 32kHz on Pin 8 (Push-Pull)
    key = AM1805_KEY_EXT; // Key 0x9D for I/O registers
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);

    i2c_transfer(AM1805_REG_CTRL2, &data, 1, true);
    data |= (1 << 2); // OUT2P = 1 (Push-Pull)
    i2c_transfer(AM1805_REG_CTRL2, &data, 1, false);

    data = 0x81; // SQWE=1, 32.768kHz frequency
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
        time->is_xt_active = !(ostat & (1 << 4));
    }
    return AM1805_OK;
}

AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buf[8], key = AM1805_KEY_EXT;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false); /* Unlock EXT registers */

    buf[0] = dec2bcd(time->hundredths);
    buf[1] = dec2bcd(time->seconds);
    buf[2] = dec2bcd(time->minutes);
    buf[3] = dec2bcd(time->hours);
    buf[4] = dec2bcd(time->day);
    buf[5] = dec2bcd(time->month);
    buf[6] = dec2bcd(time->year % 100);
    buf[7] = 0; /* Weekday */

    return i2c_transfer(AM1805_REG_HUNDREDTHS, buf, 8, false);
}

AM1805_Status AM1805_AutoCalibrate(float temp) {
    if (temp < RTC_MIN_VALID_TEMP) return AM1805_ERROR;
    float dt = temp - 25.0f;
    float drift = -0.034f * (dt * dt);
    int8_t adj = (int8_t)(-drift / 2.03f);

    uint8_t key = AM1805_KEY_OSC;
    i2c_transfer(AM1805_REG_KEY, &key, 1, false);
    return i2c_transfer(AM1805_REG_CAL_XT, (uint8_t*)&adj, 1, false);
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
             time->is_xt_active ? "XT" : "RC_FAULT");
}

AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time) {
    AM1805_Time_t t = {0};
    char m_str[4];
    int d, y, hh, mm, ss;
    const char *m_list[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    if (sscanf(date, "%s %d %d", m_str, &d, &y) == 3 && sscanf(time, "%d:%d:%d", &hh, &mm, &ss) == 3) {
        for (int i=0; i<12; i++) {
            if (!strcmp(m_str, m_list[i])) { t.month = i + 1; break; }
        }
        t.day = d; t.year = y; t.hours = hh; t.minutes = mm; t.seconds = ss;
    }
    return t;
}
