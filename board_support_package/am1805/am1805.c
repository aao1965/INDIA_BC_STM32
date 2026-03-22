#include "am1805.h"
#include <string.h>
#include <stdio.h>

static I2C_HandleTypeDef *p_hi2c = NULL;

/* --- Private Helpers --- */
static uint8_t bin2bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
static uint8_t bcd2bin(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

bool AM1805_Init(I2C_HandleTypeDef *hi2c_ptr) {
    p_hi2c = hi2c_ptr;
    uint8_t data;

    if (HAL_I2C_IsDeviceReady(p_hi2c, AM1805_ADDR, 3, 100) != HAL_OK) return false;

    /* Unlock configuration registers */
    data = 0x9D;
    HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_KEY, 1, &data, 1, 100);

    /* OCTRL (0x1C): OMODE=0 (XT start), FOS=1 (Auto RC Fallback) */
    data = 0x08;
    HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_OCTRL, 1, &data, 1, 100);

    /* Clear Oscillator Failure (OF) flag in Status register */
    HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_STATUS, 1, &data, 1, 100);
    data &= ~0x04;
    HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_STATUS, 1, &data, 1, 100);

    return true;
}

AM1805_Status AM1805_GetTime(AM1805_Time_t *time) {
    uint8_t raw[7];
    if (HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_HUNDREDTHS, 1, raw, 7, 100) == HAL_OK) {
        time->hundredths = bcd2bin(raw[0]);
        time->seconds    = bcd2bin(raw[1] & 0x7F);
        time->minutes    = bcd2bin(raw[2] & 0x7F);
        time->hours      = bcd2bin(raw[3] & 0x3F);
        time->day        = bcd2bin(raw[4] & 0x3F);
        time->month      = bcd2bin(raw[5] & 0x1F);
        time->year       = 2000 + bcd2bin(raw[6]);

        /* Read Oscillator Status (0x1D) */
        uint8_t osc_stat = 0;
        HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_OSC_STATUS, 1, &osc_stat, 1, 100);
        time->is_xt_active = (osc_stat & 0x10) == 0; // Bit 4 (OMODE): 0=XT, 1=RC

        return AM1805_OK;
    }
    return AM1805_ERROR;
}

/*AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t raw[7];
    raw[0] = bin2bcd(time->hundredths);
    raw[1] = bin2bcd(time->seconds);
    raw[2] = bin2bcd(time->minutes);
    raw[3] = bin2bcd(time->hours);
    raw[4] = bin2bcd(time->day);
    raw[5] = bin2bcd(time->month);
    raw[6] = bin2bcd(time->year % 100);
    return HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_HUNDREDTHS, 1, raw, 7, 100) == HAL_OK ? AM1805_OK : AM1805_ERROR;
}*/

AM1805_Status AM1805_SetTime(AM1805_Time_t *time) {
    uint8_t raw[7];
    uint8_t data;

    /* 1. Unlock configuration registers with the Magic Key */
    data = 0x9D;
    if (HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_KEY, 1, &data, 1, 100) != HAL_OK) {
        return AM1805_ERROR;
    }

    /* 2. Enable Writing to RTC registers (Set WRTC bit in Control1) */
    /* We read current CTRL1, set bit 0, and write it back */
    if (HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_CTRL1, 1, &data, 1, 100) != HAL_OK) {
        return AM1805_ERROR;
    }
    data |= 0x01; /* Set WRTC bit */
    if (HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_CTRL1, 1, &data, 1, 100) != HAL_OK) {
        return AM1805_ERROR;
    }

    /* 3. Prepare BCD data for time registers */
    raw[0] = bin2bcd(time->hundredths);
    raw[1] = bin2bcd(time->seconds);
    raw[2] = bin2bcd(time->minutes);
    raw[3] = bin2bcd(time->hours);
    raw[4] = bin2bcd(time->day);
    raw[5] = bin2bcd(time->month);
    raw[6] = bin2bcd(time->year % 100);

    /* 4. Write the time data */
    if (HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_HUNDREDTHS, 1, raw, 7, 100) != HAL_OK) {
        return AM1805_ERROR;
    }

    /* 5. Optional: Disable WRTC bit to protect against accidental writes */
    data &= ~0x01;
    HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_CTRL1, 1, &data, 1, 100);

    return AM1805_OK;
}

AM1805_Status AM1805_AutoCalibrate(float temp) {
    int8_t adj = 0;
    float dt = temp - 25.0f;

    // Check current source via OMODE bit in OSC_STATUS register
    uint8_t osc_stat = 0;
    HAL_I2C_Mem_Read(p_hi2c, AM1805_ADDR, AM1805_REG_OSC_STATUS, 1, &osc_stat, 1, 100);
    bool is_xt = (osc_stat & 0x10) == 0;

    if (is_xt) {
        /* Crystal: Parabolic drift. 1 step = 2.03 ppm */
        float drift_ppm = 0.034f * dt * dt;
        adj = (int8_t)(drift_ppm / 2.03f);
        return HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_CAL_XT, 1, (uint8_t*)&adj, 1, 100) == HAL_OK ? AM1805_OK : AM1805_ERROR;
    } else {
        /* RC: Linear drift (~-1.5 ppm/C). 1 step = 1.72 ppm */
        float drift_ppm = -1.5f * dt;
        adj = (int8_t)(drift_ppm / 1.72f);
        return HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_CAL_RC, 1, (uint8_t*)&adj, 1, 100) == HAL_OK ? AM1805_OK : AM1805_ERROR;
    }
}

AM1805_Status AM1805_Test_EnableClkOut(void) {
    uint8_t key = 0x9D;
    uint8_t ctrl1 = 0x00; // Enable CLKOUT
    HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_KEY, 1, &key, 1, 100);
    return HAL_I2C_Mem_Write(p_hi2c, AM1805_ADDR, AM1805_REG_CTRL1, 1, &ctrl1, 1, 100) == HAL_OK ? AM1805_OK : AM1805_ERROR;
}

AM1805_Time_t AM1805_ParseBuildTime(const char* date, const char* time) {
    AM1805_Time_t t = {0};
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    for (int i = 0; i < 12; i++) {
        if (memcmp(&date[0], &months[i * 3], 3) == 0) { t.month = i + 1; break; }
    }
    t.day = (uint8_t)((date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0'));
    t.year = (uint16_t)((date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0'));
    t.hours   = (uint8_t)((time[0] - '0') * 10 + (time[1] - '0'));
    t.minutes = (uint8_t)((time[3] - '0') * 10 + (time[4] - '0'));
    t.seconds = (uint8_t)((time[6] - '0') * 10 + (time[7] - '0'));
    t.is_xt_active = true; // Assume XT initially
    return t;
}

/**
 * @brief Converts RTC structure to string "HH:MM:SS [XT/RC]"
 */
void AM1805_FormatTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (buf == NULL || time == NULL) return;

    snprintf(buf, len, "%02d:%02d:%02d [%s]",
             time->hours,
             time->minutes,
             time->seconds,
             time->is_xt_active ? "XT" : "RC");
}


/**
 * @brief Formats full date and time into the provided buffer.
 * @encoding UTF-8
 */
void AM1805_FormatFullDateTime(char* buf, uint16_t len, AM1805_Time_t* time) {
    if (buf == NULL || time == NULL || len < 20) return;

    /* Format: 2026-03-22 17:45:30 */
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
             time->year,
             time->month,
             time->day,
             time->hours,
             time->minutes,
             time->seconds);
}
