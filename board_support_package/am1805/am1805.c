#include "am1805.h"
#include <stdio.h>
#include <string.h>

static I2C_HandleTypeDef *p_hi2c = NULL;
static osMutexId_t i2c_mutex = NULL;

/* Вспомогательные макросы BCD */
static inline uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static inline uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

static AM1805_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL) return AM1805_ERROR;
    
    if (i2c_mutex != NULL && osMutexAcquire(i2c_mutex, AM1805_MUTEX_TIMEOUT) != osOK) {
        return AM1805_BUSY;
    }

    HAL_StatusTypeDef res = is_read ? 
        HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT) :
        HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, reg, 1, pData, size, AM1805_I2C_TIMEOUT);

    if (i2c_mutex != NULL) osMutexRelease(i2c_mutex);
    return (res == HAL_OK) ? AM1805_OK : AM1805_ERROR;
}

bool AM1805_Init_Smart(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key, ctrl1, osc_stat;

    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, AM1805_I2C_TIMEOUT) != HAL_OK) return false;

    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true) != AM1805_OK) return false;
    if (i2c_transfer(AM1805_REG_OSC_STAT, &osc_stat, 1, true) != AM1805_OK) return false;

    // Пропуск настройки, если часы уже запущены (WRTC=0) и нет сбоев генератора (OF=0)
    if (((ctrl1 & 0x01) == 0) && ((osc_stat & 0x02) == 0)) {
        return true; 
    }

    // Сброс флага OF
    data = 0x00;
    if (i2c_transfer(AM1805_REG_OSC_STAT, &data, 1, false) != AM1805_OK) return false;

    // Включение XT кварца (0x1C)
    key = AM1805_KEY_OSC;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x00;
    if (i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false) != AM1805_OK) return false;

    // BREF = 2.1V (0x21)
    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0xB0;
    if (i2c_transfer(AM1805_REG_BREF, &data, 1, false) != AM1805_OK) return false;

    // BATMODE_IO: IOBM = 1 (0x27)
    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x80;
    if (i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false) != AM1805_OK) return false;

    // Запуск счета: сброс WRTC, режим 24h (0x10)
    ctrl1 &= ~(0x01 | 0x40);
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false) != AM1805_OK) return false;

    return true;
}

AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t buffer[8], osc_stat;

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
    
    // XPF (бит 4): 0 = активен XT
    time->is_xt_active = (osc_stat & 0x10) ? false : true;

    return AM1805_OK;
}

AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t buffer[8], ctrl1;
    AM1805_Status status;
    
    if ((status = i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true)) != AM1805_OK) return status;

    ctrl1 |= 0x01; // WRTC=1
    if ((status = i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false)) != AM1805_OK) return status;

    buffer[0] = dec2bcd(time->hundredths);
    buffer[1] = dec2bcd(time->seconds);
    buffer[2] = dec2bcd(time->minutes);
    buffer[3] = dec2bcd(time->hours);
    buffer[4] = dec2bcd(time->day);
    buffer[5] = dec2bcd(time->month);
    buffer[6] = dec2bcd(time->year % 100);
    buffer[7] = time->weekday & 0x07;

    if ((status = i2c_transfer(AM1805_REG_HUNDREDTHS, buffer, 8, false)) != AM1805_OK) return status;

    ctrl1 &= ~0x01; // WRTC=0
    return i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false);
}

AM1805_Status AM1805_GetDiagnostics(AM1805_Diag_t *diag) {
    AM1805_Status status;

    if ((status = i2c_transfer(AM1805_REG_STATUS, &diag->status, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_CTRL1, &diag->ctrl1, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_CAL_XT, &diag->cal_xt, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_OSC_CTRL, &diag->osc_ctrl, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_OSC_STAT, &diag->osc_stat, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_BREF, &diag->bref, 1, true)) != AM1805_OK) return status;
    if ((status = i2c_transfer(AM1805_REG_BATMODE_IO, &diag->batmode_io, 1, true)) != AM1805_OK) return status;

    return AM1805_OK;
}

AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time) {
    AM1805_Time_t t = {0};
    char m_str[4]; 
    int d, y, h, mn, s;

    sscanf(time, "%d:%d:%d", &h, &mn, &s);
    t.hours = h; t.minutes = mn; t.seconds = s;

    sscanf(date, "%3s %d %d", m_str, &d, &y);
    t.day = d; t.year = y;

    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    t.month = 1;
    for (int i = 0; i < 12; i++) {
        if (strncmp(m_str, months[i], 3) == 0) { 
            t.month = i + 1; 
            break; 
        }
    }
    t.weekday = 1; 
    return t;
}

bool AM1805_ForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password) {
    if (password != 0x55AA3C0F) return false;

    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;
    uint8_t data, key, ctrl1;

    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 5, AM1805_I2C_TIMEOUT) != HAL_OK) return false;

    data = 0x00; 
    if (i2c_transfer(AM1805_REG_OSC_STAT, &data, 1, false) != AM1805_OK) return false;

    key = AM1805_KEY_OSC;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x00;
    if (i2c_transfer(AM1805_REG_OSC_CTRL, &data, 1, false) != AM1805_OK) return false;

    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0xB0; 
    if (i2c_transfer(AM1805_REG_BREF, &data, 1, false) != AM1805_OK) return false;

    key = AM1805_KEY_CONF;
    if (i2c_transfer(AM1805_REG_KEY, &key, 1, false) != AM1805_OK) return false;
    data = 0x80; 
    if (i2c_transfer(AM1805_REG_BATMODE_IO, &data, 1, false) != AM1805_OK) return false;

    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, true) != AM1805_OK) return false;
    ctrl1 &= ~(0x01 | AM1805_CTRL1_24H); 
    if (i2c_transfer(AM1805_REG_CTRL1, &ctrl1, 1, false) != AM1805_OK) return false;

    return true;
}

AM1805_Status AM1805_SoftwareReset(void) {
    uint8_t reset_cmd = AM1805_KEY_RESET;
    AM1805_Status status = i2c_transfer(AM1805_REG_KEY, &reset_cmd, 1, false);
    osDelay(10);

    uint8_t clear_status = 0x00;
    i2c_transfer(AM1805_REG_STATUS, &clear_status, 1, false);

    return status;
}

bool AM1805_ResetAndForceInit(I2C_HandleTypeDef *hi2c_ptr, osMutexId_t mutex_id, uint32_t password) {
    p_hi2c = hi2c_ptr;
    i2c_mutex = mutex_id;

    if (AM1805_SoftwareReset() != AM1805_OK) return false;
    return AM1805_ForceInit(hi2c_ptr, mutex_id, 0x55AA3C0F);
}

AM1805_Status AM1805_SetCalibrationByDrift(float drift_sec_per_hour) {
    float ppm_error = (drift_sec_per_hour / 3600.0f) * 1000000.0f;
    float padj = -ppm_error;
    int16_t adj = (int16_t)(padj / 1.90735f);

    uint8_t xtcal = 0, cmdx = 0;
    int8_t offsetx = 0;

    if (adj < -320) {
        xtcal = 3; cmdx = 1; offsetx = -64;
    } else if (adj < -256) {
        xtcal = 3; cmdx = 1; offsetx = (adj + 192) / 2;
    } else if (adj < -192) {
        xtcal = 3; cmdx = 0; offsetx = adj + 192;
    } else if (adj < -128) {
        xtcal = 2; cmdx = 0; offsetx = adj + 128;
    } else if (adj < -64) {
        xtcal = 1; cmdx = 0; offsetx = adj + 64;
    } else if (adj < 64) {
        xtcal = 0; cmdx = 0; offsetx = adj;
    } else if (adj < 128) {
        xtcal = 0; cmdx = 1; offsetx = adj / 2;
    } else {
        xtcal = 0; cmdx = 1; offsetx = 63;
    }

    AM1805_Status status;
    uint8_t reg_1d = 0;

    status = i2c_transfer(0x1D, &reg_1d, 1, true);
    if (status != AM1805_OK) return status;

    reg_1d = (reg_1d & 0x3F) | (xtcal << 6);
    status = i2c_transfer(0x1D, &reg_1d, 1, false);
    if (status != AM1805_OK) return status;

    uint8_t reg_14 = (uint8_t)(offsetx & 0x7F);
    if (cmdx == 1) reg_14 |= 0x80;

    return i2c_transfer(AM1805_REG_CAL_XT, &reg_14, 1, false);
}

void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (!buf || !time) return;
    snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u [%s]",
             time->year, time->month, time->day,
             time->hours, time->minutes, time->seconds,
             time->is_xt_active ? "XT" : "RC/ERR");
}
