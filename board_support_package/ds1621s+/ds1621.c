#include "ds1621.h"

static osMutexId_t i2c_mutex = NULL;
static I2C_HandleTypeDef *p_hi2c = NULL; 
static float last_temp = -999.0f;
static const uint32_t I2C_TIMEOUT = 200;

/**
 * @brief Returns the mutex handle for sharing the bus with other devices like AM1805.
 */
osMutexId_t DS1621_GetI2CMutex(void) {
    return i2c_mutex;
}

/**
 * @brief Internal helper for thread-safe I2C memory operations.
 */
static DS1621_Status i2c_transfer(uint8_t reg, uint8_t *pData, uint16_t size, bool is_read) {
    if (p_hi2c == NULL || i2c_mutex == NULL) return DS1621_ERROR_INIT;
    
    /* Mutex timeout (300ms) is higher than HAL I2C timeout (200ms) to avoid race conditions */
    if (osMutexAcquire(i2c_mutex, 300) != osOK) return DS1621_ERROR_BUSY;

    HAL_StatusTypeDef res;
    if (is_read) {
        res = HAL_I2C_Mem_Read(p_hi2c, DS1621_ADDR, reg, I2C_MEMADD_SIZE_8BIT, pData, size, I2C_TIMEOUT);
    } else {
        res = HAL_I2C_Mem_Write(p_hi2c, DS1621_ADDR, reg, I2C_MEMADD_SIZE_8BIT, pData, size, I2C_TIMEOUT);
    }

    osMutexRelease(i2c_mutex);
    return (res == HAL_OK) ? DS1621_OK : DS1621_ERROR_I2C;
}

/**
 * @brief Global initialization. Should be called once during system startup (e.g. in BSP).
 */
bool DS1621_Init(I2C_HandleTypeDef *hi2c_ptr) {
    p_hi2c = hi2c_ptr;

    /* Create mutex only if it doesn't exist yet */
    if (i2c_mutex == NULL) {
        i2c_mutex = osMutexNew(NULL);
    }
    if (i2c_mutex == NULL) return false;

    /* Configure sensor to One-Shot mode */
    uint8_t cfg = DS1621_BIT_1SHOT;
    if (i2c_transfer(DS1621_REG_CONFIG, &cfg, 1, false) != DS1621_OK) return false;

    /* Wait for EEPROM write cycle (max 500ms) */
    osDelay(500); 
    return true;
}

/**
 * @brief Reads temperature registers and calculates 0.0625°C precision value.
 * Correctly handles negative temperatures.
 */
DS1621_Status DS1621_GetTemperatureHighRes(float *temperature) {
    uint8_t raw[2], count, slope;

    if (i2c_transfer(DS1621_REG_TEMP, raw, 2, true) != DS1621_OK) return DS1621_ERROR_I2C;
    i2c_transfer(DS1621_REG_COUNT, &count, 1, true);
    i2c_transfer(DS1621_REG_SLOPE, &slope, 1, true);

    if (slope == 0) return DS1621_ERROR_I2C;

    /* Casting raw[0] to int8_t ensures proper sign extension for negative values */
    float t_read = (float)((int8_t)raw[0]);
    
    /* High Resolution Formula: T = T_read - 0.25 + (Slope - Count) / Slope */
    *temperature = t_read - 0.25f + ((float)(slope - count) / (float)slope);

    return DS1621_OK;
}

/**
 * @brief Logic for a single measurement step: Trigger -> Wait -> Read.
 */
DS1621_Status DS1621_Update(void) {
    uint8_t cmd_start = DS1621_CMD_START;
    uint8_t cfg_status;
    float current;
    DS1621_Status res;

    /* 1. Trigger One-Shot conversion */
    if (osMutexAcquire(i2c_mutex, 100) == osOK) {
        HAL_I2C_Master_Transmit(p_hi2c, DS1621_ADDR, &cmd_start, 1, I2C_TIMEOUT);
        osMutexRelease(i2c_mutex);
    }

#ifdef DS1621_USE_DONE_POLLING
    /* 2a. Poll the DONE bit with 1 second total timeout */
    bool is_done = false;
    for (int i = 0; i < 10; i++) {
        osDelay(100);
        if (i2c_transfer(DS1621_REG_CONFIG, &cfg_status, 1, true) == DS1621_OK) {
            if (cfg_status & DS1621_BIT_DONE) { 
                is_done = true; 
                break; 
            }
        }
    }
    if (!is_done) return DS1621_TIMEOUT;
#else
    /* 2b. Fixed delay based on maximum conversion time (750ms) */
    osDelay(800);
#endif

    /* 3. Retrieve and store calculated data */
    res = DS1621_GetTemperatureHighRes(&current);
    if (res == DS1621_OK) {
        last_temp = current;
    }
    return res;
}

/**
 * @brief Returns the last successfully measured temperature.
 */
float DS1621_GetLastTemperature(void) {
    return last_temp;
}

/**
 * @brief Background task cycle. Initialization must be done before this starts.
 */
void DS1621_Task(void *argument) {
    for (;;) {
        DS1621_Update();
        osDelay(200); 
    }
}
/**
 * @brief Create the DS1621 OS Thread.
 */
osThreadId_t DS1621_CreateTask(void) {
    const osThreadAttr_t attr = {
        .name = "DS1621_Task", 
        .stack_size = 256 * 4, 
        .priority = osPriorityNormal
    };
    return osThreadNew(DS1621_Task, NULL, &attr);
}
